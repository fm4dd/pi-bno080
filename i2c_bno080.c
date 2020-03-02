/* ------------------------------------------------------------ *
 * file:        i2c_bno080.c                                    *
 * purpose:     Extract sensor data from Bosch BNO080 modules.  *
 *              Functions for I2C bus communication, get and    *
 *              set sensor register data. Ths file belongs to   *
 *              the pi-bno080 package. Functions are called     *
 *              from getbno080.c, globals are in getbno080.h.   *
 *                                                              *
 * Requires:	I2C development packages i2c-tools libi2c-dev   *
 *                                                              *
 * author:      07/14/2018 Frank4DD                             *
 * ------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include "getbno080.h"

uint32_t readu32(uint8_t *p) {
   uint32_t retval = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
   return retval;
}
uint16_t read16(uint8_t *p) {
   int16_t retval = p[0] | (p[1] << 8);
   return retval;
}
uint8_t read8(uint8_t *p) {
   int8_t retval = p[0];
   return retval;
}


/* ------------------------------------------------------------ *
 * Given the data packet, send the header and then the data.    *
 * ------------------------------------------------------------ */
void sendPacket(short channel, short datalen) {
   uint8_t packetlen = datalen;
   uint8_t *data;               // local buffer for I2C write data

   sequence[channel]++;         // increment seq for each packet
   packetlen = datalen + 4;     // Add four bytes for the header
   data = malloc(packetlen);    // 4-byte packet header
   data[0] = packetlen & 0xFF;  // packet length LSB
   data[1] = packetlen >> 8;    // packet length MSB
   data[2] = channel;           // channel number
   data[3] = sequence[channel]; // packet sequence num

   // Copy the payload data from shtpData to the I2C data buffer
   for (short i = 0 ; i < datalen; i++) {
      data[4+i] = shtpData[i];
   }

   if(verbose == 1) printf("Debug: TX %3d bytes HEAD", packetlen);
   for (short i = 0 ; i < packetlen; i++) {
      if(verbose == 1 && i == 4) printf(" CARGO");
      if(verbose == 1) printf(" %02X", data[i]);
   }
   if(verbose == 1) printf("\n");

   if(write(i2cfd, data, packetlen) != packetlen) {
      printf("Error: I2C write failure %d data\n", packetlen);
      exit(-1);
   }
}

/* ------------------------------------------------------------ *
 * Check to see if there is any new data available. Read the    *
 * contents of the incoming packet into the shtpData array.     *
 * Returns cargo data size in bytes, or 0 for errors.           *
 * ------------------------------------------------------------ */
int receivePacket(void) {
   int rbytes;                // Received bytes uffer
   int err;                   // error code buffer
   uint8_t subtransfer = 0;   // if not all data is read in one go,
                              // the header byte 2 MSB is set

   // 1st Read to get the 4-byte SHTP header with the cargo size
   rbytes = read(i2cfd, shtpHeader, 4);
   err = errno;

   if(rbytes != 4) {
      printf("Error: I2C SHTP header read failure: %d.\n", rbytes);
      printf("Error: %s\n", strerror(err));
      return(0);
   }

   // Calculate the number of data bytes to be received
   short packetlen = ((short) shtpHeader[1] << 8 | shtpHeader[0]);
   packetlen &= ~(1 << 15);       // Clear the MSbit.
   short datalen = packetlen - 4; // Remove the 4 header bytes
   // Check if the subtransfer bit was set (shtpHeader[1] MSB)
   if(shtpHeader[1]&0x80) subtransfer = 1;

   // 2nd Read the remaining cargo data
   if(datalen == 0) {             // Cargo data is to be received
      if(verbose == 1) printf("Debug: No SHTP data available at this time.\n");
      return(0);
   }

   uint8_t data[packetlen];    // Buffer for cargo data
   usleep(1000);               // Wait 100 microsecs before next read

   rbytes = read(i2cfd, data, packetlen);
   err = errno;

   if(rbytes < packetlen) {
      printf("Error: I2C SHTP data read failure: got %d/%d bytes.\n", rbytes, datalen);
      printf("Error: %s\n", strerror(err));
      return(0);
   }

   // update the sequence counter for the channel
   sequence[data[2]] = data[3];
   //if(verbose == 1) printf("Debug: Sequence channel %d is %d\n",
   //                         data[2], sequence[data[2]]);

   // clear global data array, and write data buffer to it
   memset(shtpData, 0, sizeof shtpData);

   if(verbose == 1) printf("Debug: RX %3d bytes HEAD", packetlen);
   for (int i = 0; i < packetlen; i++) {
      if(verbose == 1 && i == 4) printf(" CARGO");
      if(verbose == 1 && i < 20) printf(" %02X", data[i]); // only first 20 bytes
      if(verbose == 1 && i > 19 && i == 20) printf(" +%d more bytes", (packetlen-16));
      if(i < 4) shtpHeader[i] = data[i]; // Store data into the shtpData array
      if(i > 3) shtpData[i-4] = data[i]; // Store data into the shtpData array
   }
   if(verbose == 1) printf(" ST [%d]\n", subtransfer);

   if(data[4] == COMMAND_RESPONSE) {
      printf("Debug: CMD reportID [%02X] REPseq [%02X] CMD [%02X] CMDseq [%02X] RESPseq [%02X] R0 [%02X]\n",
              data[4], data[5], data[6], data[7],data[8],data[9]);

   }
   return(datalen);
}

/* ------------------------------------------------------------ *
 * shtp_init() - Enables the I2C bus communication. Raspberry   *
 * Pi 2 uses i2c-1, RPI 1 used i2c-0, NanoPi also uses i2c-0.   *
 * ------------------------------------------------------------ */
void shtp_init(char *i2cbus, char *i2caddr) {

   if((i2cfd = open(i2cbus, O_RDWR)) < 0) {
      printf("Error failed to open I2C bus [%s].\n", i2cbus);
      exit(-1);
   }
   if(verbose == 1) printf("Debug: I2C bus device: [%s]\n", i2cbus);
   /* --------------------------------------------------------- *
    * Set I2C device (BNO080 I2C address is  0x4a or 0x4b)      *
    * --------------------------------------------------------- */
   int addr = (int)strtol(i2caddr, NULL, 16);
   if(verbose == 1) printf("Debug: Sensor address: [0x%02X]\n", addr);

   if(ioctl(i2cfd, I2C_SLAVE, addr) != 0) {
      printf("Error can't find sensor at I2C address [0x%02X].\n", addr);
      exit(-1);
   }
   usleep(I2CDELAY);

   /* --------------------------------------------------------- *
    * Check if there is an unsolicited packet from power-up     *
    * 1. packet: unsolicited advertising packet (chan 0)        *
    *  RX 276 bytes HEAD 14 81 00 01 CARGO 00 01 04 00 00 00 00 *
    *                                                           *
    * if the system just powered up, the ad packet is waiting.  *
    * If we write a CMD before reading, error 0B gets recorded. *
    * However if we try to read and there is no packet waiting, *
    * the sensor crashes (no I2C response).                     *
    * So we do a write to get the error list. If this was the 
    * initial power-up, the write creates one error, in which  *
    * case we do reset for a clean state. For subsequent calls  *
    * the error lost should be empty and we don't need tp reset *
    * --------------------------------------------------------- */
   int errorcount = get_shtp_errors();
   if(errorcount > 0) bno_reset(addr);
   if(verbose == 1) printf("Debug: OK  Initialization complete\n");
}

int get_shtp_errors() {
   // Test code: Below line simulates an SHTP error for incomplete
   // header data. SH2 will add the code 2 entry to the error list
   // sensor reset clears the error list.
   // char data[3] = { 2, 3, 4}; write(i2cfd, data, 3);

   /* --------------------------------------------------------- *
    * SHTP get error list from sensor                           *
    * --------------------------------------------------------- */
   shtpData[0] = 0x01;                // CMD 0x01 gets error list
   sendPacket(CHANNEL_COMMAND, 1);    // Write 1 byte to chan CMD
   usleep(I2CDELAY);                  // 100 msecs before next I2C

   /* --------------------------------------------------------- *
    * Get the SHTP error list, after reset it should be clean   *
    *  RX   5 bytes HEAD 05 80 00 03 CARGO 01 ST [0]            *
    * --------------------------------------------------------- */
   // Wait for answer packet
   int datalen;
   while ((datalen = receivePacket()) != 0) {
      if(shtpHeader[2] == CHANNEL_COMMAND && shtpData[0] == 0x01) break;
      usleep(I2CDELAY);              // Delay 100 microsecs before next I2C
   }

   if(shtpHeader[2] != CHANNEL_COMMAND || shtpData[0] != 1) {
      printf("Error: can't get SHTP error list\n");
      return(-1);
   }

   /* --------------------------------------------------------- *
    * Calculate the error counter                               *
    * --------------------------------------------------------- */
   int errcount = datalen - 1; // datalen minus 1 report byte
   if(verbose == 1) printf("Debug: OK  Error list %d entries\n", errcount);

   return(errcount);
}

/* ------------------------------------------------------------ *
 * print_shtp_errors() prints the error strings from error list *
 * This function needs to be called after get_shtp_errors().    *
 * ------------------------------------------------------------ */
int print_shtp_errors() {
const static char* shtpErrorStr[] = {
   "No Error",
   "Hub application attempted to exceed maximum read cargo length",
   "Hub application attempted to exceed maximum read cargo length",
   "Host wrote a header with length greater than maximum write cargo length",
   "Host wrote a header with length <= header length (invalid or no payload)",
   "Host tried to fragment cargo (transfer length < full cargo length)",
   "Host wrote continuation of fragmented cargo (continuation bit set)",
   "Unrecognized command on control channel (2)",
   "Unrecognized parameter to get-advertisement command",
   "Host wrote to unrecognized channel",
   "Advertisement request received while Advertisement Response was pending",
   "Host write before the hub finished sending advertisement response",
   "Error list too long to send, truncated"
};

   /* --------------------------------------------------------- *
    * Check if shtpHeader and Data contain the error list       *
    * --------------------------------------------------------- */
   if(shtpHeader[2] != CHANNEL_COMMAND || shtpData[0] != 0x01) {
      if(verbose == 1) printf("Debug: Error list not read\n");
      return 1;
   }

   /* --------------------------------------------------------- *
    * Calculate the error counter                               *
    * --------------------------------------------------------- */
   short packetlen = ((short) shtpHeader[1] << 8 | shtpHeader[0]);
   packetlen &= ~(1 << 15);        // Clear the MSbit.
   short errcount = packetlen - 5; // minus 4 header and 1 report

   printf("SHTP Errors # : %d entries\n", errcount);
   if(errcount == 0) return(0);

   for(int i=0; i<errcount; i++) {
      printf("SHTP Error %2d : %d = %s\n", i, shtpData[1+i], shtpErrorStr[shtpData[1+i]]);
   }

   return(0);
}

/* --------------------------------------------------------------- *
 * bno_dump() dumps the flash record system (FRS) data to screen   *
 * --------------------------------------------------------------- */
int bno_dump() {
   int count = 0;

   printf("------------------------------------------------------\n");
   printf("BNO080 page-0:\n");
   printf("------------------------------------------------------\n");
   printf(" reg    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
   printf("------------------------------------------------------\n");
   while(count < 8) {
      char reg = count;
      if(write(i2cfd, &reg, 1) != 1) {
         printf("Error: I2C write failure for register 0x%02X\n", reg);
         exit(-1);
      }

      char data[16] = {0};
      if(read(i2cfd, &data, 16) != 16) {
         printf("Error: I2C read failure for register 0x%02X\n", reg);
         exit(-1);
       
      }
      printf("[0x%02X] %02X %02X %02X %02X %02X %02X %02X %02X",
             (reg*16), data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
      printf(" %02X %02X %02X %02X %02X %02X %02X %02X\n",
             data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
      count++;
   }

   usleep(50 * 1000);
   count = 0;
   printf("------------------------------------------------------\n");
   printf("BNO080 page-1:\n");
   printf("------------------------------------------------------\n");
   printf(" reg    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
   printf("------------------------------------------------------\n");
   while(count < 8) {
      char reg = count;
      if(write(i2cfd, &reg, 1) != 1) {
         printf("Error: I2C write failure for register 0x%02X\n", reg);
         exit(-1);
      }

      char data[16] = {0};
      if(read(i2cfd, &data, 16) != 16) {
         printf("Error: I2C read failure for register 0x%02X\n", reg);
         exit(-1);
       
      }
      printf("[0x%02X] %02X %02X %02X %02X %02X %02X %02X %02X",
             (reg*16), data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
      printf(" %02X %02X %02X %02X %02X %02X %02X %02X\n",
             data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
      count++;
   }

   exit(0);
}

/* ------------------------------------------------------------ *
 * bno_reset() resets the sensor.                               *
 * ------------------------------------------------------------ */
void bno_reset() {
   /* --------------------------------------------------------- *
    * Send the "reset" command and watch the response packets   *
    * --------------------------------------------------------- */
   shtpData[0] = 1;                   // CMD1 = reset
   sendPacket(CHANNEL_EXECUTABLE, 1); // Write 1 byte to chan EXE
   usleep(700000);                    // 700 millisecs for reboot

   /* --------------------------------------------------------- *
    * After reset,  we get 3 packets:                           *
    * 1. packet: unsolicited advertising packet (chan 0)        *
    * --------------------------------------------------------- */
   receivePacket();
   if(shtpHeader[2] != CHANNEL_COMMAND || shtpHeader[3] != 1) {
      printf("Error: can't get SHTP advertising.\n");
      exit(-1);
   }
   usleep(I2CDELAY);            // Delay 100 microsecs before next I2C

   /* --------------------------------------------------------- *
    * 2. packet: "reset complete" (chan 1, response code 1)     *
    * --------------------------------------------------------- */
   receivePacket();
   if(shtpHeader[2] != CHANNEL_EXECUTABLE || shtpHeader[3] != 1 
      || shtpData[0] != 1) {
      printf("Error: can't get 'reset complete' status.\n");
      exit(-1);
   }
   usleep(I2CDELAY);            // Delay 100 microsecs before next I2C
   /* --------------------------------------------------------- *
    * 3. packet: SH-2 sensor hub SW init (chan 2)               *
    * --------------------------------------------------------- */
   receivePacket();
   if(shtpHeader[2] != CHANNEL_CONTROL || shtpHeader[3] != 1) {
      printf("Error: can't get SH2 initialization.\n");
      exit(-1);
   }

   if(verbose == 1) printf("Debug: OK  Reset complete\n");
}

/* ------------------------------------------------------------ *
 * get_calstat() gets the calibration status from the sensor.   *
 * ------------------------------------------------------------ */
int get_calstat(struct bnocal *bno_ptr) {
   /* --------------------------------------------------------- *
    * Get ME Calibration Command SH-2 reference manual 6.4.7.2  *
    * --------------------------------------------------------- */
   cmdsequence++;
   shtpData[0] = COMMAND_REQUEST;   // CMD request
   shtpData[1] = cmdsequence;       // CMD sequence number
   shtpData[2] = 0x07;              // ME Calibration CMD 0x07
   shtpData[3] = 0x00;              // Reserved
   shtpData[4] = 0x00;              // Reserved
   shtpData[5] = 0x00;              // Reserved
   shtpData[6] = 0x01;              // Reserved
   shtpData[7] = 0x00;              // Get ME Calibration 0x01
   shtpData[8] = 0x00;              // Reserved
   shtpData[9] = 0x00;              // Reserved
   shtpData[10] = 0x00;             // Reserved
   shtpData[11] = 0x00;             // Reserved
   sendPacket(CHANNEL_CONTROL, 12); // Write 12 bytes to CMD channel
   usleep(I2CDELAY);                // Delay 100 microsecs befoer next I2C

   // Wait for answer packet
   int datalen;
   while ((datalen = receivePacket()) != 0) {
      if(shtpHeader[2] == CHANNEL_CONTROL && shtpData[0] == 0xF1) break;
      usleep(I2CDELAY);              // Delay 100 microsecs before next I2C
   }

   if(shtpData[0] != 0xF1) {
      printf("Error: Getting command response\n");
      exit(-1);
   }
   if(shtpData[2] != 0x07) {
      printf("Error: Getting ME calibration response\n");
      exit(-1);
   }
   if(verbose == 1) printf("Debug: OK  ME calibration received, data [%02X]\n",
                            shtpData[2]);

   bno_ptr->acal_st = shtpData[6]; // accelerometer calibration status
   bno_ptr->gcal_st = shtpData[7]; // gyroscope calibration status
   bno_ptr->mcal_st = shtpData[8]; // magnetometer calibration status
   bno_ptr->pcal_st = shtpData[9]; // planar accel calibration status
   if(verbose == 1) {
      printf("Debug: calibration enable settings");
      printf(" acc=[%d]", bno_ptr->acal_st);
      printf(" gyr=[%d]", bno_ptr->gcal_st);
      printf(" mag=[%d]", bno_ptr->mcal_st);
      printf(" planar_acc=[%d]\n", bno_ptr->pcal_st);
   }
   return(0);
}

/* ------------------------------------------------------------ *
 * Calibration offset is stored in 3x6 (18) registers 0x55~0x66 *
 * plus 4 registers 0x67~0x6A accelerometer/magnetometer radius *
 * ------------------------------------------------------------ */
int get_caloffset(struct bnocal *bno_ptr) {
   /* --------------------------------------------------------- *
    * Registers may not update in fusion mode, switch to CONFIG *
    * --------------------------------------------------------- */

   /* ------------------------------------------------------------ *
    * assigning accelerometer X-Y-Z offset, range per G-range      *
    * 16G = +/-16000, 8G = +/-8000, 4G = +/-4000, 2G = +/-2000     *
    * ------------------------------------------------------------ */
   return(0);
}

/* ------------------------------------------------------------ *
 * save_cal() - writes calibration data to file for reuse       *
 * ------------------------------------------------------------ */
int save_cal(char *file) {
   /* --------------------------------------------------------- *
    * Read 34 bytes calibration data from registers 0x43~66,    *
    * plus 4 reg 0x67~6A with accelerometer/magnetometer radius *
    * switch to CONFIG, data is only visible in non-fusion mode *
    * --------------------------------------------------------- */
   return(0);
}

/* ------------------------------------------------------------ *
 * load_cal() load previously saved calibration data from file  *
 * ------------------------------------------------------------ */
int load_cal(char *file) {
   /* -------------------------------------------------------- *
    *  Open the calibration data file for reading.             *
    * -------------------------------------------------------- */
   FILE *calib;
   if(! (calib=fopen(file, "r"))) {
      printf("Error: Can't open %s for reading.\n", file);
      exit(-1);
   }
   if(verbose == 1) printf("Debug: Load from file: [%s]\n", file);

   /* -------------------------------------------------------- *
    * Read 34 bytes from file into data[], starting at data[1] *
    * -------------------------------------------------------- */
  // char data[CALIB_BYTECOUNT+1] = {0};
   //data[0] = ACC_OFFSET_X_LSB_ADDR;
   //data[0] = BNO080_SIC_MATRIX_0_LSB_ADDR;
   //int inbytes = fread(&data[1], 1, CALIB_BYTECOUNT, calib);
   //fclose(calib);

   return(0);
}

/* ------------------------------------------------------------ *
 * print_unit() - Extract the SI unit config from register 0x3B *
 * ------------------------------------------------------------ */
void print_unit(int unit_sel) {
   // bit-0
   printf("Acceleration Unit  = ");
   if((unit_sel >> 0) & 0x01) printf("mg\n");
   else printf("m/s2\n");

   // bit-1
   printf("    Gyroscope Unit = ");
   if((unit_sel >> 1) & 0x01) printf("rps\n");
   else printf("dps\n");

   // bit-2
   printf("        Euler Unit = ");
   if((unit_sel >> 2) & 0x01) printf("Radians\n");
   else printf("Degrees\n");

   // bit-3: unused
   // bit-4
   printf("  Temperature Unit = ");
   if((unit_sel >> 4) & 0x01) printf("Fahrenheit\n");
   else printf("Celsius\n");

   // bit-5: unused
   // bit-6: unused
   // bit-7
   printf("  Orientation Mode = ");
   if((unit_sel >> 3) & 0x01) printf("Android\n");
   else printf("Windows\n");
}

/* ------------------------------------------------------------ *
 * get_prodid() queries the BNO080 and write the info data into *
 * the global prodid struct bnoinf defined in getbno080.h       *
 * ------------------------------------------------------------ */
int get_prodid(struct prodid prodlist[]) {
   /* --------------------------------------------------------- *
    * SHTP communication channel 2: write 0xF9 request          *
    * --------------------------------------------------------- */
   shtpData[0] = PRODUCT_ID_REQUEST;
   sendPacket(CHANNEL_CONTROL, 1);  // Write 1 bytes to CTL channel
   usleep(I2CDELAY);                // Delay 100 microsecs befoer next I2C

   /* --------------------------------------------------------- *
    * SHTP communication channel 2: read 1st 0xF8 response      *
    * --------------------------------------------------------- */
   short count = 0;
   int datalen = 0;
   while ((datalen = receivePacket()) >0) {
      if(count > 3) break;
      if(shtpHeader[2] == CHANNEL_CONTROL
         && shtpData[0] == PRODUCT_ID_RESPONSE) break;
      usleep(I2CDELAY);             // Delay 100 microsecs befoer next I2C
      count++;
   }

   if(shtpData[0] != PRODUCT_ID_RESPONSE) {
      printf("Error: Not getting 1st SHTP product-ID report\n");
      exit(-1);
   }
   if(verbose == 1) printf("Debug: OK  1st SHTP product-ID report received, [%d bytes]\n",
                            datalen);

   /* --------------------------------------------------------- *
    * Assign 1st product report data to info structure          *
    * --------------------------------------------------------- */
   prodlist[0].rep_id  = read8(&shtpData[0]) ;  // report 0xF8 byte 0 Report ID
   prodlist[0].r_cause = read8(&shtpData[1]);   // report 0xF8 byte 1 Reset Cause
   prodlist[0].sw_vmaj = read8(&shtpData[2]);   // report 0xF8 byte 2 SW Version Major
   prodlist[0].sw_vmin = read8(&shtpData[3]);   // report 0xF8 byte 2 SW Version Minor
   prodlist[0].sw_pnm  = readu32(&shtpData[4]); // report 0xF8 byte 4-7 SW Part Number
   prodlist[0].sw_bnm  = readu32(&shtpData[8]); // report 0xF8 byte 8-11 SW Build Number
   prodlist[0].sw_vpn  = read16(&shtpData[12]); // report 0xF8 byte 12-13 SW Version Patch

   /* --------------------------------------------------------- *
    * SHTP communication channel 2: read 2nd 0xF8 response      *
    * --------------------------------------------------------- */
   usleep(200000);             // Delay 100 microsecs befoer next I2C
   count = 0;
   datalen = 0;
   while ((datalen = receivePacket()) >0) {
      if(count > 3) break;
      if(shtpHeader[2] == CHANNEL_CONTROL
         && shtpData[0] == PRODUCT_ID_RESPONSE) break;
      usleep(I2CDELAY);                 // Delay 100 microsecs befoer next I2C
      count++;
   }
      printf("Test %d data-0 %d\n", count, shtpData[0]);

   if(shtpData[0] != PRODUCT_ID_RESPONSE) {
      printf("Error: Not getting 2nd SHTP product-ID report\n");
      exit(-1);
   }
   if(verbose == 1) printf("Debug: OK  2nd SHTP product-ID report received, [%d bytes]\n",
                            datalen);
   /* --------------------------------------------------------- *
    * Assign 2nd product report data to info structure          *
    * --------------------------------------------------------- */
   prodlist[1].rep_id  = read8(&shtpData[0]) ;  // report 0xF8 byte 0 Report ID
   prodlist[1].r_cause = read8(&shtpData[1]);   // report 0xF8 byte 1 Reset Cause
   prodlist[1].sw_vmaj = read8(&shtpData[2]);   // report 0xF8 byte 2 SW Version Major
   prodlist[1].sw_vmin = read8(&shtpData[3]);   // report 0xF8 byte 2 SW Version Minor
   prodlist[1].sw_pnm  = readu32(&shtpData[4]); // report 0xF8 byte 4-7 SW Part Number
   prodlist[1].sw_bnm  = readu32(&shtpData[8]); // report 0xF8 byte 8-11 SW Build Number
   prodlist[1].sw_vpn  = read16(&shtpData[12]); // report 0xF8 byte 12-13 SW Version Patch

   return(0);
}

/* ------------------------------------------------------------ *
 * get_frs() - Read sensor metadata from flash into shtpData[]  *
 * SH-2 reference manual 5.1                                    *
 * ------------------------------------------------------------ */
int get_frs(int recid) {
   short count = 0;
   int datalen = 0;
   /* --------------------------------------------------------- *
    * convert recid into LSB and MSB                            *
    * --------------------------------------------------------- */
   char lbyte = recid & 0x00FF;
   char hbyte = recid >> 8;
   /* --------------------------------------------------------- *
    * SHTP FRS read request 0xF4 / 0xF3 response CTL Channel 2  *
    * --------------------------------------------------------- */
   shtpData[0] = FRS_READ_REQUEST;
   shtpData[1] = 0x00;
   shtpData[2] = 0x00;
   shtpData[3] = 0x00;
   shtpData[4] = lbyte;
   shtpData[5] = hbyte;
   shtpData[6] = 0x00;
   shtpData[7] = 0x00;
   sendPacket(CHANNEL_CONTROL, 8);  // Write 8 bytes to CTL channel
   usleep(I2CDELAY);                // Delay 100 microsecs before next I2C

   count = 0;
   datalen = 0;
   while ((datalen = receivePacket()) != 0) {
      if(count > 3) break;
      if(shtpHeader[2] == CHANNEL_CONTROL
         && shtpData[0] == FRS_READ_RESPONSE) break;
      usleep(I2CDELAY);              // Delay 100 microsecs before next I2C
      count++;
   }

   if(shtpData[0] != FRS_READ_RESPONSE) {
      printf("Error: Not getting SHTP feature report\n");
      exit(-1);
   }
   if(verbose == 1) printf("Debug: FRS response report received, [%d bytes]\n",
                            datalen);
   if(verbose == 1) printf("[%02X] [%02X] [%02X] [%02X]\n", shtpData[16],shtpData[17], shtpData[18],shtpData[19]);

   //usleep(10000);
   //datalen = receivePacket();

   return(0);
}

int get_serial(double *serial) {
   int res = get_frs(0xE312);
   
   return(res);
}

/* ------------------------------------------------------------ *
 * get_acc() - Read acceleration data and save it into bnoacc   *
 * SH-2 reference manual 6.5.8.2, format figure 72              *
 * ------------------------------------------------------------ */
int get_acc(struct bnoacc *bnod_ptr) {
   short count = 0;
   int datalen = 0;
   /* --------------------------------------------------------- *
    * Check if ACC is already enabled, if not enable it now...  *
    * --------------------------------------------------------- */
   shtpData[0] = SET_FEATURE_COMMAND;
   shtpData[1] = 0x01;
   shtpData[5] = 0x60;
   shtpData[6] = 0xEA;
   sendPacket(CHANNEL_CONTROL, 17); // Write 16 bytes to CTL channel
   usleep(200000);                   // wait 200 millisecs for completion

   datalen = receivePacket();
   usleep(I2CDELAY);

   shtpData[0] = GET_FEATURE_REQUEST;
   shtpData[1] = 0x01;
   sendPacket(CHANNEL_CONTROL, 2); // Write 20 bytes to CTL channel
   usleep(I2CDELAY);               // Delay 100 msecs before next I2C

   count = 0;
   datalen = 0;
   while ((datalen = receivePacket()) != 0) {
      if(count > 3) break;
      if(shtpHeader[2] == CHANNEL_CONTROL
         && shtpData[0] == GET_FEATURE_RESPONSE) break;
      usleep(I2CDELAY);             // Delay 100 msecs before next I2C
      count++;
   }

   if(shtpData[0] != GET_FEATURE_RESPONSE || 
      shtpData[1] != 0x01) {
      printf("Error: Not getting SHTP feature report\n");
      exit(-1);
   }
   if(verbose == 1) printf("Debug: FRS feature report received, [%d bytes]\n",
                            datalen);
   if(verbose == 1) printf("[%02X] [%02X] [%02X] [%02X]\n", shtpData[16],shtpData[17], shtpData[18],shtpData[19]);

   if(shtpData[16] == 0x00) return(1);
   usleep(200000);                   // wait 200 millisecs for completion
   datalen = receivePacket();
   return(0);
}


/* ------------------------------------------------------------ *
 *  get_eul() - read Euler orientation into the global struct   *
 * SH-2 reference manual 3.1.3 H-format output packet layout    *
 * ------------------------------------------------------------ *
 * H-Packet (18 bytes), enabled through FRS (flash) setting:    *
 * 00 - Header LSB                                              *
 * 01 - Header MSB    16-bit packet header, always 0xAAAA       *
 * 02 - Sequence      0-255 with roll-over                      *
 * 03 - yaw LSB (Z)                                             *
 * 04 - yaw MSB (Z)   16-bit yaw in centidegrees (degx100)      *
 * 05 - pitch LSB (Y)                                           *
 * 06 - pitch MSB (Y) 16-bit pitch in centidegrees (degx100)    *
 * 07 - roll LSB (X)                                            *
 * 08 - roll MSB (X)  16-bit roll in centidegrees (degx100)     *
 * 09 - X-acc LSB (Z)                                           *
 * 10 - X-acc MSB (Z) 16-bit X-accel in milligravities (Gx1000) *
 * 11 - Y-acc LSB (Y)                                           *
 * 12 - Y-acc MSB (Y) 16-bit Y-accel in milligravities (Gx1000) *
 * 13 - Z-acc LSB (X)                                           *
 * 14 - Z-acc MSB (X) 16-bit Z-accel in milligravties (Gx1000)  *
 * 15 - Reserved, always 0x00                                   *
 * 16 - Reserved, always 0x00                                   *
 * 17 - Reserved, always 0x00                                   *
 * 18 - Checksum, sum of all 16 bytes from 02-18 (excl. header) *
 * ------------------------------------------------------------ */
int get_eul(struct bnoeul *bnod_ptr) {
   int16_t buf = read16(&shtpData[3]);
   if(verbose == 1) printf("Debug: Euler Orientation Y: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", shtpData[3], shtpData[4],buf);
   bnod_ptr->eul_head = (double) buf / 100.0; // convert centidegrees to degrees

   buf = read16(&shtpData[5]); 
   if(verbose == 1) printf("Debug: Euler Orientation P: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", shtpData[5], shtpData[6],buf);
   bnod_ptr->eul_roll = (double) buf / 100.0; // convert centidegrees to degrees

   buf =  read16(&shtpData[7]); 
   if(verbose == 1) printf("Debug: Euler Orientation R: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", shtpData[7], shtpData[8],buf);
   bnod_ptr->eul_pitc = (double) buf / 100.0; // convert centidegrees to degrees
   return(0);
}

/* ------------------------------------------------------------ *
 *  get_qua() - read Quaternation data into the global struct   *
 * ------------------------------------------------------------ */
int get_qua(struct bnoqua *bnod_ptr) {
   char data[100] = { '\0' };
   int16_t buf = ((int16_t)data[1] << 8) | data[0]; 
   if(verbose == 1) printf("Debug: Quaternation W: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[0], data[1],buf);
   bnod_ptr->quater_w = (double) buf / 16384.0;

   buf = ((int16_t)data[3] << 8) | data[2]; 
   if(verbose == 1) printf("Debug: Quaternation X: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[2], data[3],buf);
   bnod_ptr->quater_x = (double) buf / 16384.0;

   buf = ((int16_t)data[5] << 8) | data[4]; 
   if(verbose == 1) printf("Debug: Quaternation Y: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[4], data[5],buf);
   bnod_ptr->quater_y = (double) buf / 16384.0;

   buf = ((int16_t)data[7] << 8) | data[6]; 
   if(verbose == 1) printf("Debug: Quaternation Z: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[6], data[7],buf);
   bnod_ptr->quater_z = (double) buf / 16384.0;
   return(0);
}

/* ------------------------------------------------------------ *
 *  get_gra() - read gravity vector into the global struct      *
 * ------------------------------------------------------------ */
int get_gra(struct bnogra *bnod_ptr) {
   char data[100] = { '\0' };
   int16_t buf = ((int16_t)data[1] << 8) | data[0];
   if(verbose == 1) printf("Debug: Gravity Vector H: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[0], data[1],buf);

   buf = ((int16_t)data[3] << 8) | data[2];
   if(verbose == 1) printf("Debug: Gravity Vector M: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[2], data[3],buf);

   buf = ((int16_t)data[5] << 8) | data[4];
   if(verbose == 1) printf("Debug: Gravity Vector P: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[4], data[5],buf);
   return(0);
}

/* ------------------------------------------------------------ *
 *  get_lin() - read linear acceleration into the global struct *
 * ------------------------------------------------------------ */
int get_lin(struct bnolin *bnod_ptr) {
   char data[100] = { '\0' };
   int16_t buf = ((int16_t)data[1] << 8) | data[0];
   if(verbose == 1) printf("Debug: Linear Acceleration H: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[0], data[1],buf);

   buf = ((int16_t)data[3] << 8) | data[2];
   if(verbose == 1) printf("Debug: Linear Acceleration M: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[2], data[3],buf);

   buf = ((int16_t)data[5] << 8) | data[4];
   if(verbose == 1) printf("Debug: Linear Acceleration P: LSB [0x%02X] MSB [0x%02X] INT16 [%d]\n", data[4], data[5],buf);
   return(0);
}

/* ------------------------------------------------------------ *
 * set_mode() - set the sensor operational mode register 0x3D   *
 * The modes cannot be switched over directly, first it needs   *
 * to be set to "config" mode before switching to the new mode. *
 * ------------------------------------------------------------ */
int set_mode() {
   return(0);
}

/* ------------------------------------------------------------ *
 * get_mode() - returns sensor operational mode register 0x3D   *
 * Reads 1 byte from Operations Mode register 0x3d, and uses    *
 * only the lowest 4 bit. Bits 4-7 are unused, stripped off     *
 * ------------------------------------------------------------ */
int get_mode() {
   unsigned int data = 0;
   if(verbose == 1) printf("Debug: Operation Mode: [0x%02X]\n", data & 0x0F);

   return(data & 0x0F);  // only return the lowest 4 bits
}

/* ------------------------------------------------------------ *
 * print_mode() - prints sensor operational mode string from    *
 * sensor operational mode numeric value.                       *
 * ------------------------------------------------------------ */
int print_mode(int mode) {
   if(mode < 0 || mode > 12) return(-1);
   
   switch(mode) {
      case 0x00:
         printf("CONFIG\n");
         break;
      case 0x01:
         printf("ACCONLY\n");
         break;
      case 0x02:
         printf("MAGONLY\n");
         break;
      case 0x03:
         printf("GYRONLY\n");
         break;
      case 0x04:
         printf("ACCMAG\n");
         break;
      case 0x05:
         printf("ACCGYRO\n");
         break;
      case 0x06:
         printf("MAGGYRO\n");
         break;
      case 0x07:
         printf("AMG\n");
         break;
      case 0x08:
         printf("IMU\n");
         break;
      case 0x09:
         printf("COMPASS\n");
         break;
      case 0x0A:
         printf("M4G\n");
         break;
      case 0x0B:
         printf("NDOF_FMC_OFF\n");
         break;
      case 0x0C:
         printf("NDOF_FMC\n");
         break;
   }
   return(0);
}

/* ------------------------------------------------------------ *
 * set_power() - set the sensor power mode in register 0x3E.    *
 * The power modes cannot be switched over directly, first the  *
 * ops mode needs to be "config"  to write the new power mode.  *
 * ------------------------------------------------------------ */
int set_power(power_t pwrmode) {
   return(0);
}

/* ------------------------------------------------------------ *
 * get_power() returns the sensor power mode from register 0x3e *
 * Only the lowest 2 bit are used, ignore the unused bits 2-7.  *
 * ------------------------------------------------------------ */
int get_power() {
   unsigned int data = 0;
   if(verbose == 1) printf("Debug:     Power Mode: [0x%02X] 2bit [0x%02X]\n", data, data & 0x03);
   return(data & 0x03);  // only return the lowest 2 bits
}

/* ------------------------------------------------------------ *
 * print_power() - prints the sensor power mode string from the *
 * sensors power mode numeric value.                            *
 * ------------------------------------------------------------ */
int print_power(int mode) {
   if(mode < 0 || mode > 2) return(-1);

   switch(mode) {
      case 0x00:
         printf("NORMAL\n");
         break;
      case 0x01:
         printf("LOW\n");
         break;
      case 0x02:
         printf("SUSPEND\n");
         break;
   }
   return(0);
}

void parseInputReport(int datalen) {

  uint8_t status = shtpData[5 + 2] & 0x03; //Get status bits
  uint16_t data1 = (uint16_t)shtpData[5 + 5] << 8 | shtpData[5 + 4];
  uint16_t data2 = (uint16_t)shtpData[5 + 7] << 8 | shtpData[5 + 6];
  uint16_t data3 = (uint16_t)shtpData[5 + 9] << 8 | shtpData[5 + 8];
  uint16_t data4 = 0;
  uint16_t data5 = 0;

  if (datalen - 5 > 9) {
    data4 = (uint16_t)shtpData[5 + 11] << 8 | shtpData[5 + 10];
  }
  if (datalen - 5 > 11) {
    data5 = (uint16_t)shtpData[5 + 13] << 8 | shtpData[5 + 12];
  }

  //Store these generic values to their proper global variable
  if (shtpData[5] == SENSOR_REPORTID_ACC) {
    accelAccuracy = status;
    rawAccelX = data1;
    rawAccelY = data2;
    rawAccelZ = data3;
  }
  else if (shtpData[5] == SENSOR_REPORTID_LIN) {
    accelLinAccuracy = status;
    rawLinAccelX = data1;
    rawLinAccelY = data2;
    rawLinAccelZ = data3;
  }
  else if (shtpData[5] == SENSOR_REPORTID_GYR) {
    gyroAccuracy = status;
    rawGyroX = data1;
    rawGyroY = data2;
    rawGyroZ = data3;
  }
  else if (shtpData[5] == SENSOR_REPORTID_MAG) {
    magAccuracy = status;
    rawMagX = data1;
    rawMagY = data2;
    rawMagZ = data3;
  }
  else if (shtpData[5] == SENSOR_REPORTID_ROT ||
           shtpData[5] == SENSOR_REPORTID_GAM) {
    quatAccuracy = status;
    rawQuatI = data1;
    rawQuatJ = data2;
    rawQuatK = data3;
    rawQuatReal = data4;
    rawQuatRadianAccuracy = data5; //Only available on rotation vector, not game rot vector
  }
  else if (shtpData[5] == SENSOR_REPORTID_STP) {
    stepCount = data3; //Bytes 8/9
  }
  else if (shtpData[5] == SENSOR_REPORTID_STA) {
    stabilityClassifier = shtpData[5 + 4]; //Byte 4 only
  }
  else if (shtpData[5] == SENSOR_REPORTID_PER) {
    activityClassifier = shtpData[5 + 5]; //Most likely state

    //Load activity classification confidences into the array
    for (uint8_t x = 0 ; x < 9 ; x++) //Hardcoded to max of 9. TODO - bring in array size
      _activityConfidences[x] = shtpData[5 + 6 + x]; //5 bytes of timestamp, byte 6 is first confidence byte
  }
  else {
    printf ("Error: sensor report ID [%02X] is unhandled.\n", shtpData[5]);
  }
}

//Given a register value and a Q point, convert to float
//See https://en.wikipedia.org/wiki/Q_(number_format)
float qToFloat(int16_t fixedPointValue, uint8_t qPoint) {
   float qFloat = fixedPointValue;
   qFloat *= pow(2, qPoint * -1);
   return (qFloat);
}
// rotation vector quaternion I
float get_quati() {
  float quat = qToFloat(rawQuatI, rotationVector_Q1);
  return (quat);
}

// rotation vector quaternion J
float get_quatj() {
   float quat = qToFloat(rawQuatJ, rotationVector_Q1);
   return (quat);
}

// rotation vector quaternion K
float get_quatk() {
   float quat = qToFloat(rawQuatK, rotationVector_Q1);
   return (quat);
}
// rotation vector quaternion Real
float get_quatreal() {
   float quat = qToFloat(rawQuatReal, rotationVector_Q1);
   return (quat);
}
