/* ------------------------------------------------------------ *
 * file:        getbno080.h                                     *
 * purpose:     header file for getbno080.c and i2c_bno080.c    *
 *                                                              *
 * author:      05/04/2018 Frank4DD                             *
 * ------------------------------------------------------------ */
#include <stdint.h>
// I2C device on old RPI and clones is 0, for RPI2 and up its 1
#define I2CBUS               "/dev/i2c-0"
// I2C packet read delay in microseconds
#define I2CDELAY             200
// Packets can be up to 32k.
#define MAX_PACKET_SIZE      32762 
// This is in words, we only care about the first 9 (Qs, range, etc)
#define MAX_METADATA_SIZE    9
// SHTP cmd channel: byte-0=command, byte-1=parameter, byte-n=parameter
#define CHANNEL_COMMAND      0
// SHTP exec channel: write 1=reset, 2=on, 3=sleep; read 1=reset complete
#define CHANNEL_EXECUTABLE   1
// Sensor Hub control channel for sensor config commands and responses
#define CHANNEL_CONTROL      2
// Input Sensor reports only sends data from sensor to host
#define CHANNEL_REPORTS      3
// Wake Input Sensor reports sends data from wake sensors to host
#define CHANNEL_WAKE_REPORTS 4
// Gyro rotation vector in extra channel to allow prioritization
#define CHANNEL_GYRO         5
// Control channel commands (BNO8X datasheet figure 1-30)
#define COMMAND_RESPONSE     0xF1
#define COMMAND_REQUEST      0xF2
#define FRS_READ_RESPONSE    0xF3  // Flash Record System read response
#define FRS_READ_REQUEST     0xF4  // Flash Record System read request
#define FRS_WRITE_RESPONSE   0xF5  // Flash Record System write response
#define FRS__WRITE_DATA      0xF6  // Flash Record System write data
#define FRS__WRITE_REQUEST   0xF7  // Flash Record System write request
#define PRODUCT_ID_RESPONSE  0xF8
#define PRODUCT_ID_REQUEST   0xF9
#define GET_TIME_REFERENCE   0xFB 
#define GET_FEATURE_RESPONSE 0xFC
#define SET_FEATURE_COMMAND  0xFD
#define GET_FEATURE_REQUEST  0xFE
//All the different sensors and features we can get reports from
//These are used when enabling a given sensor
#define SENSOR_REPORTID_ACC 0x01 // Accelerometer
#define SENSOR_REPORTID_GYR 0x02 // Gyroscope
#define SENSOR_REPORTID_MAG 0x03 // Magnetometer
#define SENSOR_REPORTID_LIN 0x04 // Linear Acceleration
#define SENSOR_REPORTID_ROT 0x05 // Rotation Vector
#define SENSOR_REPORTID_GRA 0x06 // Gravity
#define SENSOR_REPORTID_GAM 0x08 // Game Rotation Vector
#define SENSOR_REPORTID_GEO 0x09 // Geomagnetic Rotation
#define SENSOR_REPORTID_TAP 0x10 // Tap Detector
#define SENSOR_REPORTID_STP 0x11 // Step Counter
#define SENSOR_REPORTID_STA 0x13 // Stability Classifier
#define SENSOR_REPORTID_PER 0x1E // Personal Activity Classifier

/* ------------------------------------------------------------ *
 * global variables                                             *
 * ------------------------------------------------------------ */
// I2C file descriptor
int i2cfd;
// debug flag, 0 = normal, 1 = debug mode
int verbose;
// Each packet has a header of 4 bytes
uint8_t shtpHeader[4];
// The data array for read and write operations
uint8_t shtpData[MAX_PACKET_SIZE];
// 6 SHTP channels. Each channel has its own seqnum
uint8_t sequence[6];
// Commands sequence number inside the command packet
uint8_t cmdsequence;
// > 10 words in a metadata, but we'll stop at Q point 3
unsigned int metaData[MAX_METADATA_SIZE];
//These are the raw sensor values pulled from the user requested Input Report
uint16_t rawAccelX, rawAccelY, rawAccelZ, accelAccuracy;
uint16_t rawLinAccelX, rawLinAccelY, rawLinAccelZ, accelLinAccuracy;
uint16_t rawGyroX, rawGyroY, rawGyroZ, gyroAccuracy;
uint16_t rawMagX, rawMagY, rawMagZ, magAccuracy;
uint16_t rawQuatI, rawQuatJ, rawQuatK, rawQuatReal, rawQuatRadianAccuracy, quatAccuracy;
uint16_t stepCount;
uint8_t stabilityClassifier;
uint8_t activityClassifier;
uint8_t *_activityConfidences; //Array that store the confidences of the 9 possible activities
uint8_t calibrationStatus; //Byte R0 of ME Calibration Response
//These Q values are defined in the datasheet but can also be obtained by querying the meta data records
//See the read metadata example for more info
int16_t rotationVector_Q1;
int16_t accelerometer_Q1;
int16_t linear_accelerometer_Q1;
int16_t gyro_Q1;
int16_t magnetometer_Q1;
/* ------------------------------------------------------------ *
 * BNO080 versions, status data and other infos struct 14 bytes *
 * ------------------------------------------------------------ */
struct prodid {
   uint8_t rep_id;   // report id 0xF8
   uint8_t r_cause;  // report 0xF8 byte 1 Reset Cause
   uint8_t sw_vmaj;  // report 0xF8 byte 2 SW Version Major
   uint8_t sw_vmin;  // report 0xF8 byte 3 SW Version Minor
   uint32_t sw_pnm;  // report 0xF8 byte 4-7 SW Part Number
   uint32_t sw_bnm;  // report 0xF8 byte 8-11 SW Build Number
   uint16_t sw_vpn;  // report 0xF8 byte 12-13 SW Version Patch
};
#define ARRAY_ITEMS(x)  (sizeof(x) / sizeof((x)[0]))

/* ------------------------------------------------------------ *
 * BNO080 calibration data struct. The offset ranges depend on  *
 * the component operation range. For example, the accelerometer*
 * range can be set as 2G, 4G, 8G, and 16G. I.e. the offset for *
 * the accelerometer at 16G has a range of +/- 16000mG. Offset  *
 * is stored on the sensor in two bytes with max value of 32768.*
 * ------------------------------------------------------------ */
struct bnocal{
   char acal_st;  // accelerometer calibration enable (1|0)
   char gcal_st;  // gyroscope calibration enable (1|0)
   char mcal_st;  // magnetometer calibration enable (1|0)
   char pcal_st;  // planar accel calibration enable (1|0)
   int  aoff_x;   // accelerometer offset, X-axis
   int  aoff_y;   // accelerometer offset, Y-axis
   int  aoff_z;   // accelerometer offset, Z-axis
   int  moff_x;   // magnetometer offset, X-axis
   int  moff_y;   // magnetometer offset, Y-axis
   int  moff_z;   // magnetometer offset, Z-axis
   int  goff_x;   // gyroscope offset, X-axis
   int  goff_y;   // gyroscope offset, Y-axis
   int  goff_z;   // gyroscope offset, Z-axis
   int acc_rad;   // accelerometer radius
   int mag_rad;   // magnetometer radius
};

/* ------------------------------------------------------------ *
 * BNO080 measurement data structs. Data gets filled in based   *
 * on the sensor component type that was requested for reading. *
 * ------------------------------------------------------------ */
struct bnoacc{
   double adata_x;   // accelerometer data, X-axis
   double adata_y;   // accelerometer data, Y-axis
   double adata_z;   // accelerometer data, Z-axis
};
struct bnoeul{
   double eul_head;  // Euler heading data
   double eul_roll;  // Euler roll data
   double eul_pitc;  // Euler picth data
};
struct bnoqua{
   double quater_x;  // Quaternation data X
   double quater_y;  // Quaternation data Y
   double quater_z;  // Quaternation data Z
   double quater_w;  // Quaternation data W
};
struct bnogra{
   double gravityx;  // Gravity Vector X
   double gravityy;  // Gravity Vector Y
   double gravityz;  // Gravity Vector Z
};
struct bnolin{
   double linacc_x;  // Linear Acceleration X
   double linacc_y;  // Linear Acceleration Y
   double linacc_z;  // Linear Acceleration Z
};

/* ------------------------------------------------------------ *
 * BNO080 accelerometer gyroscope magnetometer config structs   *
 * ------------------------------------------------------------ */
struct bnoaconf{
   int pwrmode;      // p-1 reg 0x08 accelerometer power mode
   int bandwth;      // p-1 reg 0x08 accelerometer bandwidth
   int range;        // p-1 reg 0x08 accelerometer rate
   int slpmode;      // p-1 reg 0x0C accelerometer sleep mode
   int slpdur;       // p-1 reg 0x0C accelerometer sleep duration
};
struct bnomconf{
   int pwrmode;      // p-1 reg 0x09 magnetometer power mode
   int oprmode;      // p-1 reg 0x09 magnetometer operation
   int outrate;      // p-1 reg 0x09 magnetometer output rate
};
struct bnogconf{
   int pwrmode;      // p-1 reg 0x0B gyroscope power mode
   int bandwth;      // p-1 reg 0x0A gyroscope bandwidth
   int range;        // p-1 reg 0x0A gyroscope range
   int slpdur;       // p-1 reg 0x0D gyroscope sleep duration
   int aslpdur;      // p-1 reg 0x0D gyroscope auto sleep dur
};

/* ------------------------------------------------------------ *
 * Operations and power mode, name to value translation         *
 * ------------------------------------------------------------ */
typedef enum {
   normal  = 0x00,
   low     = 0x01,
   suspend = 0x02
} power_t;

/* ------------------------------------------------------------ *
 * external function prototypes for I2C bus communication code  *
 * ------------------------------------------------------------ */
extern void shtp_init(char*);             // Start I2C and SHTP msgs
extern int set_page0();                   // set register map page 0
extern int set_page1();                   // set register map page 1
extern int get_calstat(struct bnocal*);   // read calibration status
extern void print_calstat(struct bnocal*);// print calibration status
extern int get_caloffset(struct bnocal*); // read calibration values
extern int get_prodid(struct prodid[]);   // read sensor information
extern int get_acc(struct bnoacc*);       // read accelerometer data
extern int get_eul(struct bnoeul*);       // read euler orientation
extern int get_qua(struct bnoqua*);       // read quaternation data
extern int get_gra(struct bnogra*);       // read gravity data
extern int get_lin(struct bnolin*);       // read linar acceleration data
extern int get_serial(double *serial);    // get the sensor serial #
extern int get_clksrc();                  // get the clock source setting
extern void print_clksrc();               // print clock source setting
extern int set_mode();                    // set the sensor ops mode
extern int get_mode();                    // get the sensor ops mode
extern int print_mode(int);               // print ops mode string
extern void print_unit(int);              // print SI unit configuration
extern int set_power(power_t);            // set the sensor power mode
extern int get_shtp_errors();             // get the shtp error list
extern int print_shtp_errors();           // print the shtp error list
extern int get_power();                   // get the sensor power mode
extern int print_power(int);              // print power mode string
extern int get_sstat();                   // get system status code
extern int print_sstat(int);              // print system status string
extern int get_remap(char);               // get the axis remap values
extern int print_remap_conf(int);         // print axis configuration
extern int print_remap_sign(int);         // print the axis remap +/-
extern int bno_dump();                    // dump the register map data
extern void bno_reset();                  // reset the sensor
extern int save_cal(char*);               // write calibration to file
extern int load_cal(char*);               // load calibration from file
extern int get_acc_conf(struct bnoaconf*);// get accelerometer config
extern int get_mag_conf(struct bnomconf*);// get magnetometer config
extern int get_gyr_conf(struct bnogconf*);// get gyroscope config
extern int set_acc_conf();                // set accelerometer config
extern int set_mag_conf();                // set magnetometer config
extern int set_gyr_conf();                // set gyroscope config
extern void print_acc_conf();             // print accelerometer config
extern void print_mag_conf();             // print magnetometer config
extern void print_gyr_conf();             // print gyroscope config
