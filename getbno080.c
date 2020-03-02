/* ------------------------------------------------------------ *
 * file:        getbno080.c                                     *
 * purpose:     Sensor control and data extraction program for  *
 *              the  Hillcrest Labs BNO080 IMU sensor board.    *
 *                                                              *
 * return:      0 on success, and -1 on errors.                 *
 *                                                              *
 * requires:	I2C headers, e.g. sudo apt install libi2c-dev   *
 *                                                              *
 * compile:	gcc -o getbno080 i2c_bno080.c getbno080.c       *
 *                                                              *
 * example:	./getbno080 -t eul  -o bno080.htm               *
 *                                                              *
 * author:      02/04/2019 Frank4DD                             *
 * ------------------------------------------------------------ */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include "getbno080.h"

/* ------------------------------------------------------------ *
 * Global variables and defaults                                *
 * ------------------------------------------------------------ */
int verbose = 0;
int outflag = 0;
int argflag = 0; // 1 dump, 2 reset, 3 load calib, 4 write calib
char opr_mode[9] = {0};
char pwr_mode[8] = {0};
char datatype[256];
char senaddr[256] = "0x4b";
char i2c_bus[256] = I2CBUS;
char htmfile[256];
char calfile[256];

/* ------------------------------------------------------------ *
 * print_usage() prints the programs commandline instructions.  *
 * ------------------------------------------------------------ */
void usage() {
   static char const usage[] = "Usage: getbno080 [-a hex i2c-addr] [-m <opr_mode>] [-t acc|gyr|mag|eul|qua|lin|gra|inf|cal] [-r] [-w calfile] [-l calfile] [-o htmlfile] [-v]\n\
\n\
Command line parameters have the following format:\n\
   -a   sensor I2C bus address in hex, Example: -a 0x4a (default: 0x4b)\n\
   -b   I2C bus to query, Example: -b /dev/i2c-1 (default)\n\
   -c   send tare calibration command. mode arguments:\n\
           rota     = use rotation vector\n\
           game     = use gaming rotation vector\n\
           geomag   = use geomagnetic rotation vector\n\
           gyroint  = use gyro-integrated rotation vector\n\
           arvr     = use ARVR-stabilized rotation vector\n\
           arvrg    = use ARVR-stabilized game rotation vector\n\
                      Note: Tare currently sets all (x-y-z) axes.\n\
   -d   dump the complete sensor register map content\n\
   -p   set sensor power mode. mode arguments:\n\
          normal    = required sensors and MCU always on (default)\n\
          low       = enter sleep mode during motion inactivity\n\
          suspend   = sensor paused, all parts put to sleep\n\
   -r   reset sensor\n\
   -t   read and output sensor data. data type arguments:\n\
           acc = Accelerometer (X-Y-Z axis values)\n\
           gyr = Gyroscope (X-Y-Z axis values)\n\
           mag = Magnetometer (X-Y-Z axis values)\n\
           qua = Orientation Q (W-X-Y-Z values as Quaternation)\n\
           inf = Sensor info (SW version and state values)\n\
           cal = Calibration data (mag, gyro and accel calibration values)\n\
   -l   enable calibration for all sensor modules\n\
   -w   stop calibration, and write calibration data to flash\n\
   -o   output sensor data to HTML table file, requires -t, Example: -o ./bno080.html\n\
   -h   display this message\n\
   -v   enable debug output\n\
\n\
Usage examples:\n\
./getbno080 -a 0x4b -t inf -v\n\
./getbno080 -t acc -v\n\
./getbno080 -t eul -o ./bno080.html\n\
./getbno080 -r\n";
   printf(usage);
}

/* ------------------------------------------------------------ *
 * parseargs() checks the commandline arguments with C getopt   *
 * ------------------------------------------------------------ */
void parseargs(int argc, char* argv[]) {
   int arg;
   opterr = 0;

   if(argc == 1) { usage(); exit(-1); }

   while ((arg = (int) getopt (argc, argv, "a:dm:p:rt:l:w:o:hv")) != -1) {
      switch (arg) {
         // arg -v verbose, type: flag, optional
         case 'v':
            verbose = 1; break;

         // arg -a + sensor address, type: string
         // mandatory, example: 0x29
         case 'a':
            if(verbose == 1) printf("Debug: arg -a, value %s\n", optarg);
            if (strlen(optarg) != 4) {
               printf("Error: Cannot get valid -a sensor address argument.\n");
               exit(-1);
            }
            strncpy(senaddr, optarg, sizeof(senaddr));
            break;

         // arg -b + I2C bus, type: string
         // optional, example: "/dev/i2c-1"
         case 'b':
            if(verbose == 1) printf("Debug: arg -b, value %s\n", optarg);
            if (strlen(optarg) >= sizeof(i2c_bus)) {
               printf("Error: invalid i2c bus argument.\n");
               exit(-1);
            }
            strncpy(i2c_bus, optarg, sizeof(i2c_bus));
            break;

         // arg -d
         // optional, dumps the complete register map data
         case 'd':
            if(verbose == 1) printf("Debug: arg -d, value %s\n", optarg);
            argflag = 1;
            break;

         // arg -m sets operations mode, type: string
         case 'm':
            if(verbose == 1) printf("Debug: arg -m, value %s\n", optarg);
            strncpy(opr_mode, optarg, sizeof(opr_mode));
            break;

         // arg -p sets power mode, type: string
         case 'p':
            if(verbose == 1) printf("Debug: arg -p, value %s\n", optarg);
            strncpy(pwr_mode, optarg, sizeof(pwr_mode));
            break;

         // arg -r
         // optional, resets sensor
         case 'r':
            if(verbose == 1) printf("Debug: arg -r, value %s\n", optarg);
            argflag = 2;
            break;

         // arg -t + sensor component, type: string
         // mandatory, example: mag (magnetometer)
         case 't':
            if(verbose == 1) printf("Debug: arg -t, value %s\n", optarg);
            if (strlen(optarg) != 3) {
               printf("Error: Cannot get valid -t data type argument.\n");
               exit(-1);
            }
            strncpy(datatype, optarg, sizeof(datatype));
            break;

         // arg -l + calibration file name, type: string
         // loads the sensor calibration from file. example: ./bno080.cal
         case 'l':
            argflag = 3;
            if(verbose == 1) printf("Debug: arg -l, value %s\n", optarg);
            strncpy(calfile, optarg, sizeof(calfile));
            break;

         // arg -w + calibration file name, type: string
         // writes sensor calibration to file. example: ./bno080.cal
         case 'w':
            argflag = 4;
            if(verbose == 1) printf("Debug: arg -w, value %s\n", optarg);
            strncpy(calfile, optarg, sizeof(calfile));
            break;

         // arg -o + dst HTML file, type: string, requires -t
         // writes the sensor output to file. example: /tmp/sensor.htm
         case 'o':
            outflag = 1;
            if(verbose == 1) printf("Debug: arg -o, value %s\n", optarg);
            strncpy(htmfile, optarg, sizeof(htmfile));
            break;

         // arg -h usage, type: flag, optional
         case 'h':
            usage(); exit(0);
            break;

         case '?':
            if(isprint (optopt))
               printf ("Error: Unknown option `-%c'.\n", optopt);
            else
               printf ("Error: Unknown option character `\\x%x'.\n", optopt);
            usage();
            exit(-1);
            break;

         default:
            usage();
            break;
      }
   }
}

int main(int argc, char *argv[]) {
   /* ---------------------------------------------------------- *
    * Process the cmdline parameters                             *
    * ---------------------------------------------------------- */
   parseargs(argc, argv);
   int res = 0;
   //rotationVector_Q1 = 14;
   //accelerometer_Q1 = 8;
   //linear_accelerometer_Q1 = 8;
   //gyro_Q1 = 9;
   //magnetometer_Q1 = 4;
   cmdsequence = 0;

   /* ----------------------------------------------------------- *
    * get current time (now), write program start if verbose      *
    * ----------------------------------------------------------- */
   time_t tsnow = time(NULL);
   if(verbose == 1) printf("Debug: ts=[%lld] date=%s", (long long) tsnow, ctime(&tsnow));

   /* ----------------------------------------------------------- *
    * Initialize SHTP command sequence numbers, start protocol    *
    * ----------------------------------------------------------- */
   sequence[0] = 0;
   sequence[1] = 0;
   sequence[2] = 0;
   sequence[3] = 0;
   sequence[4] = 0;
   sequence[5] = 0;
   shtp_init(i2c_bus, senaddr);

   /* ----------------------------------------------------------- *
    *  "-r" reset the sensor and exit the program                 *
    * ----------------------------------------------------------- */
   if(argflag == 2) {
      bno_reset();
      exit(0);
   }

   /* ----------------------------------------------------------- *
    *  "-t acc " reads accelerometer data from the sensor.        *
    * ----------------------------------------------------------- */
   if(strcmp(datatype, "acc") == 0) {
      struct bnoacc bnod;
      res = get_acc(&bnod);
      if(res != 0) {
         printf("Error: Cannot read accelerometer data.\n");
         exit(-1);
      }

      /* ----------------------------------------------------------- *
       * print the formatted output string to stdout (Example below) *
       * ACC -45.00 264.00 939.00 (ACC X Y Z)                        *
       * ----------------------------------------------------------- */
      printf("ACC %3.2f %3.2f %3.2f\n", bnod.adata_x, bnod.adata_y, bnod.adata_z);

      if(outflag == 1) {
         /* -------------------------------------------------------- *
          *  Open the html file for writing accelerometer data       *
          * -------------------------------------------------------- */
         FILE *html;
         if(! (html=fopen(htmfile, "w"))) {
            printf("Error open %s for writing.\n", htmfile);
            exit(-1);
         }
         fprintf(html, "<table><tr>\n");
         fprintf(html, "<td class=\"sensordata\">Accelerometer X:<span class=\"sensorvalue\">%3.2f</span></td>\n", bnod.adata_x);
         fprintf(html, "<td class=\"sensorspace\"></td>\n");
         fprintf(html, "<td class=\"sensordata\">Accelerometer Y:<span class=\"sensorvalue\">%3.2f</span></td>\n", bnod.adata_y);
         fprintf(html, "<td class=\"sensorspace\"></td>\n");
         fprintf(html, "<td class=\"sensordata\">Accelerometer Z:<span class=\"sensorvalue\">%3.2f</span></td>\n", bnod.adata_z);
         fprintf(html, "</tr></table>\n");
         fclose(html);
      }
   } /* End reading Accelerometer */

   /* ----------------------------------------------------------- *
    * -t "inf"  print the sensor configuration                    *
    * ----------------------------------------------------------- */
   if(strcmp(datatype, "inf") == 0) {
      /* -------------------------------------------------- *
       *  Get the sensors software version data             *
       * -------------------------------------------------- */
      struct prodid prodlist[2];
      res = get_prodid(prodlist);
      if(res != 0) {
         printf("Error: Cannot read SW version data.\n");
         exit(-1);
      }
      /* ----------------------------------------------------- *
       *  Get the sensors calibration state                    *
       * ----------------------------------------------------- */
      struct bnocal bnoc;
      res = get_calstat(&bnoc);
      if(res != 0) {
         printf("Error: Cannot read calibration state.\n");
         exit(-1);
      }
      /* ----------------------------------------------------- *
       *  Get the sensors serial number from flash             *
       * ----------------------------------------------------- */
      double serial;
      res = get_serial(&serial);
      if(res != 0) {
         printf("Error: Cannot read calibration state.\n");
         exit(-1);
      }
      /* ----------------------------------------------------- *
       *  Get the sensors SHTP error list                      *
       * ----------------------------------------------------- */
      res = get_shtp_errors();
      if(res < 0) {
         printf("Error: Cannot read error list.\n");
         exit(-1);
      }

      /* -------------------------------------------------- *
       * print the formatted output strings to stdout       *
       * -------------------------------------------------- */
      printf("\nBNO080 Information at %s", ctime(&tsnow));
      printf("----------------------------------------------\n");

      for(int i = 0; i<ARRAY_ITEMS(prodlist); i++) {

      printf("Part %d : Version %d.%d.%d Build %d ", prodlist[i].sw_pnm,
              prodlist[i].sw_vmaj, prodlist[i].sw_vmin,
              prodlist[i].sw_vpn, prodlist[i].sw_bnm);
      printf("Last Reset: ");
      switch(prodlist[i].r_cause) {
         case 0:
            printf("Not Applicable\n");
            break;
         case 1:
            printf("Power On Reset\n");
            break;
         case 2:
            printf("Internal System Reset\n");
            break;
         case 3:
            printf("Watchdog Timeout\n");
            break;
         case 4:
            printf("External Reset\n");
            break;
         case 5:
            printf("Other\n");
            break;
         }
      }

      print_calstat(&bnoc);
      print_shtp_errors();
      printf("Sensor Serial : [%f]\n", serial);
   } // end -t inf

   exit(res);
}
/* ----------------------------------------------------------- *
 *  print_calstat() - Read and print calibration status        *
 * ----------------------------------------------------------- */
void print_calstat(struct bnocal *bno_ptr) {
   printf("Calib. ON/OFF : ACC=");
   if(bno_ptr->acal_st == 1) printf("ON");
   else printf("OFF");
   printf(" GYRO=");
   if(bno_ptr->gcal_st == 1) printf("ON");
   else printf("OFF");
   printf(" MAG=");
   if(bno_ptr->mcal_st == 1) printf("ON");
   else printf("OFF\n");
   printf(" PLANAR-ACC=");
   if(bno_ptr->pcal_st == 1) printf("ON\n");
   else printf("OFF\n");
}
