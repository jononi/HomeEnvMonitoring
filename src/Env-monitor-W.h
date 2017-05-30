/*

Header file with imports and definitions

*/

/*
      Libraries
*/

// #include "HTU21D.h"
#include "Si7021_Particle.h"
#include "TSL2561.h"
#include "MAX17043.h"
#include "HttpClient_fast.h"

/*
      Definitions
*/
// PIR sensor pin
#define pirDetectPin A4
#define pirEnablePin A5

#define sensePeriod   10000  // defines frequency of taking measurements
#define publishPeriod 25000 //defines frequency of publishing to the cloud (Particle, Ubidots...) in ms (min 1000)
#define minSleepingDuration 25000
#define maxAwakeDuration 5000

// Ubidots cloud services API params
#define ubiToken "ubiTokenHere"

#define ubiTempVarId "ubiTemperatureVariableHere"
#define ubiHumVarId "ubiHunidityVariableHere"
#define ubiIllVarId "ubiIlluminanceVariableHere"
#define ubiStatusVarId "ubiStatusVariableHere"
#define ubiBattP "ubiBatteryChargeVariableHere"
#define ubiBattV "ubiBatteryVoltageVariableHere"

/*
      Variables
*/

// print info to Serial print switch for debug purpose
// #define serial_debug

// sensors objects
TSL2561 tsl = TSL2561(TSL2561_ADDR);
// HTU21D htu = HTU21D();
Si7021 si7021;

// battery monitor is declared in its header file

// status report and execution control
// sensors state
bool tsl_opsOn = false;
bool ht_opsOn = false;
bool batt_opsOn = false;
bool pir_opsOn = false;

// variables to display these settings
char sensors_status[41];
char tsl_settings[21];
char cloud_settings[41];

// timer variables
uint32_t sendTime;
uint32_t senseTime;
uint32_t lastAwakeTime;
uint32_t sleepingDuration;

// cloud sending state
bool particleOn = false;
bool ubidotsOn = false;
bool influxdbOn = true;

// Light sensor settings vars
uint16_t integrationTime;
bool autoGainOn;

// pir vars
bool pir_calibration_on = false;

// sensors vars

//sensors vars
double illuminance = 0;

double humidity = 0;
double temperatureC = 0;

double battery_p = 0;
double voltage = 0;
bool   low_voltage_alert;

int pir_event_cnt;

// data publishing vars
char data_c[191];
char ubidots_cnt[21] = "0/0";
char influx_cnt[21] = "0/0";

// tags variables for influxdb
//playroom, master_br, kitchen, loft, family_r....
char location[20] = "default";
char humidifier[6] = "na";
// struct to store last used tags values in emulated EEPROM (E3)
struct TagsE3 {
  char  location[20];
  char  humidifier[6];
  int   version;
};

// init ubidots counter
int dots_sent = 0;
int dots_successful=0;

// init influxdb counter
int meas_sent = 0;
int meas_successful=0;

// http request instance
HttpClient http;

// ubidots http request header  (validated)
http_header_t ubiHeaders[] ={
  { "Content-Type", "application/json" },
  { "X-Auth-Token" , ubiToken },
  { NULL, NULL } // NOTE: Always terminate headers with NULL
};

// ubidots http request header  (validated)
http_header_t influxdb_H[] ={
    { "Accept" , "*/*"},
    { "User-agent", "Particle HttpClient"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};

// http requests instances
http_request_t ubidotsRequest;
http_request_t influxdbRequest;

// one http response instance used for both APIs
http_response_t response;
