/*
* Home environement monitoring and logging: Environment Monitoring for jonoryWave Photon
* Env-monitor-W
Jaafar Ben-Abdallah / June '17

TODO:

1- debug PIR usage --> done
2- replace delay(sensingPeriod) by a sleep of 25 sec or less --> done
3- create 2 modes: always on and energy saving. Switch between modes using SETUP button --> done
*/

// imports, definitions and global variables go here:
#include "Env-monitor-W.h"

void setup()
{

  // cloud variables (max 20)
  Particle.variable("temperature", temperatureC);
  Particle.variable("humidity", humidity);
  Particle.variable("illuminance", illuminance);
  // Particle.variable("voltage",voltage);
  Particle.variable("percentage",battery_p);
  Particle.variable("sen_status", sensors_status);
  // Particle.variable("tsl_settings", tsl_settings);
  // Particle.variable("pir_status", pir_calibration_on);
  Particle.variable("pir_counter", pir_event_cnt);
  // Particle.variable("ubi_http_cnt",ubidots_cnt);
  Particle.variable("influx_cnt",influx_cnt);
  Particle.variable("cloud_status",cloud_settings);

  // cloud  functions (max 12)
  Particle.function("resetSensors", resetSensors);
  Particle.function("setCloud", setActiveCloud);
  Particle.function("setMeta", setConditions);
  Particle.function("getMeta", getConditions);
  Particle.function("setEcoMode", setEnergySaving);

  // initiate cloud connection (semi-auto mode)
  Particle.connect();

  // register an event handler for SETUP button click
  System.on(button_click, buttonHandler);
  pinMode(D7, OUTPUT);

  #ifdef serial_debug
  delay(3000);
  Log.info("Starting up...");
  Serial.println("print to serial works too");
  #endif

  // initialize sensors and get status
  tsl_on = enableTSL();
  batt_on = enableBatt();
  // ht_on = enableBME();
  ht_on = si7021.begin();
  pir_on = enablePir();
  // init pir events counter
  pir_event_cnt = 0;

  sprintf(sensors_status,"Ill: %i, TH: %i, PIR: %i, Batt: %i",tsl_on, ht_on, pir_on, batt_on);
  Particle.publish("EnvW/Status", sensors_status);

  sprintf(cloud_settings,"particle: %i, influx: %i", particle_on, influxdb_on);

  // ubidots connection params
  /*
  ubidotsRequest.hostname = "things.ubidots.com";
  ubidotsRequest.port = 80;
  ubidotsRequest.path = "/api/v1.6/collections/values";
  */

  // InfluxDB server connection params
  influxdbRequest.hostname = "influxdb_server_url";
  influxdbRequest.port = 8086;
  influxdbRequest.path = "/write?db=dbName&rp=retention-policy-name&precision=s&u=influxusername&p=influxpassword";

  // uncomment next line only when need to reset Emulated EEPROM
  //EEPROM.clear();

  // read last used tags value from emulated EEPROM
  TagsE3 stored_tags;

  EEPROM.get(0, stored_tags);

  strcpy(location, stored_tags.location);
  strcpy(humidifier, stored_tags.humidifier);

  /*
  #ifdef serial_debug
  Log.info("Initialization completed");
  Log.info(sensors_status);
  Serial.printlnf("si7021 id: %d", si7021.checkID());
  Serial.printlnf("cell gauge ver: %d", lipo.getVersion());

  // get local IP
  Log.info("LocalIP: %s", WiFi.localIP());

  Log.info("resolving rpi1.nrjy.com :");
  Log.info(WiFi.resolve("rpi1.nrjy.com"));

  // get mac address
  byte mac[6];
  WiFi.macAddress(mac);
  for (int i=0; i<6; i++) {
    if (i) Log.info(":");
    Log.info(mac[i], HEX);
  }

  Log.info("free memory= %i",System.freeMemory());

  Log.info("location from E2:");
  Log.info(stored_tags.location);
  Log.info("humidifier from E2:");
  Log.info(stored_tags.humidifier);
  Log.info("version from E2:");
  Log.info(stored_tags.version);

  #endif
  */

  state = AWAKE_STATE;

  // start timers
  sleepingDuration = minSleepingDuration;
  lastWakeUpTime = millis();

}//setup


void loop()
{
  switch(state) {
		case AWAKE_STATE:
    // AWAKE state actions
    readSensors();

    // set logging flags by default if service is disabled
    if (!particle_on) particle_logged = true;
    if (!influxdb_on) influxdb_logged = true;

    // log only when at least one env sensor is enabled
    if ((tsl_on || ht_on)) {
      if (particle_on && !particle_logged && Particle.connected()) {
        Particle.publish("EnvW/data",data_c);
        delay(200);
        particle_logged = true;
      }
      if (influxdb_on && !influxdb_logged && WiFi.ready()) {
        sendInfluxdb();
        // reset PIR events counter if logged to InfluxDB
        if (pir_event_cnt > 0) pir_event_cnt = 0;
        influxdb_logged = true;
      }
    }

    // AWAKE state transition
    // awake --> inactive or sleep
    if (((millis() - lastWakeUpTime) > maxAwakeDuration) || (influxdb_logged && particle_logged)) {
      if (save_mode_on) state = SLEEP_STATE;
      else state = INACTIVE_STATE;
      // reset logging flags
      particle_logged = false;
      influxdb_logged = false;
      // set sleeping duration period (ms)
      sleepingDuration = minSleepingDuration + maxAwakeDuration - (millis() - lastWakeUpTime);
      // start sleep state timer (s)
      sleepStartTime_s = Time.now();

      #ifdef serial_debug
      Log.info("time spent awake: %i ms", millis() - lastWakeUpTime);
      Log.info("sleep duration: %i ms", sleepingDuration);
      Log.info("inactive/sleep start time: %i ", sleepStartTime_s);
      Log.info("eco mode status: %i",save_mode_on);
      #endif
    }
		break;

    case INACTIVE_STATE:
    // INACTIVE state actions
    if (pir_on && !pir_calibration_on && digitalRead(pirDetectPin) == HIGH) {
      pir_event_cnt ++;
      delay(3000); // debounce period for PIR sensing (synchronous, non blocking)
      #ifdef serial_debug
      Log.info("End of PIR sensor debounce period");
      #endif
    }

    // INACTIVE state transitions
    // inactive --> sleep
    if (save_mode_on) {
      // power save mode recently requested by setup button click or through Cloud function
      state = SLEEP_STATE;
;
      // adjust remaining sleeping duration
      sleepingDuration = sleepingDuration - 1000 * (Time.now() - sleepStartTime_s);

      #ifdef serial_debug
      Log.info("switch: inactive -> sleep after: %i s", Time.now() - sleepStartTime_s);
      Log.info("adjusted sleep duration: %i ms", sleepingDuration);
      #endif

      // adjust starting time for SLEEP state
      sleepStartTime_s = Time.now();
    }
    // inactive --> awake
    if (1000 * (Time.now() - sleepStartTime_s) >  sleepingDuration ) {
      state = AWAKE_STATE;

      #ifdef serial_debug
      Log.info("switch: inactive -> awake after: %i s", Time.now() - sleepStartTime_s);
      #endif

      lastWakeUpTime = millis();
    }

		break;

    case SLEEP_STATE:
    // SLEEP state actions
    #ifdef serial_debug
    Log.info("Going into STOP mode at: %i s for %i s", Time.now(), sleepingDuration / 1000);
    delay(10); // allow serial buffer to be emptied
    #endif

    // simulate sleep when uncommented
    /*
    Particle.disconnect();
    WiFi.off();
    delay(sleepingDuration); // synchronous, blocking PIR event detection
    WiFi.on();
    Particle.connect();
    */

    // Turn WiFi off so it doesn't reconnect when exiting STOP mode
    WiFi.off();
    //Go into STOP mode
    System.sleep(pirDetectPin, RISING, sleepingDuration / 1000);

    #ifdef serial_debug
    // SerialLogHandler logHandler(115200, LOG_LEVEL_INFO);
    Serial.begin(115200);
    Log.info("exited STOP mode after %i s", Time.now() - sleepStartTime_s);
    #endif

    // if PIR trigger caused wake-up, increment event counter and adjust sleepingDuration
    if ( (Time.now() - sleepStartTime_s) <  sleepingDuration / 1000 ) {
      if (digitalRead(pirDetectPin) == HIGH ) {
        pir_event_cnt++;
        #ifdef serial_debug
        Log.info("PIR event counter incremented to: %i", pir_event_cnt);
        #endif
      }
      sleepingDuration = sleepingDuration - 1000 * (Time.now() - sleepStartTime_s);
      sleepStartTime_s = Time.now();
    }

    // SLEEP state transitions
    // sleep --> awake
    if ( Time.now() - sleepStartTime_s >=  sleepingDuration / 1000) {
      state = AWAKE_STATE;
      #ifdef serial_debug
      Log.info("switch sleep -> awake after: %i s", Time.now() - sleepStartTime_s);
      #endif
      // Now is a good time to reconnect
      // WiFi.on();
      // Particle.connect();
      lastWakeUpTime = millis();
    }

		break;
  }

  // notification for power save mode switch
  if (save_mode_switched) {
    save_mode_switched = false;
    digitalWrite(D7, LOW);
    blink_on_switch();
  }
}


/* **********************************************************
*              Sensors Functions
*  ********************************************************** */

void readSensors() {
  // ****************** PIR sensor ******************
  // check if PIR calibration phase is over
  if (pir_calibration_on == true && digitalRead(pirDetectPin) == LOW) {
    // calibration is over
    pir_calibration_on = false;
    Particle.publish("EnvW/PIR","End Cal");
    // Particle.publish("EnvD/PIR","End Cal");
  }
  // PIR sensor events are only aggregated in INACTIE or SLEEP modes

  /*
  if (pir_on && !pir_calibration_on && pir_event_cnt == 0) {
    if (digitalRead(pirDetectPin) == HIGH ) {
      // this event was not detected by GPIO interrupt during sleep phase
      pir_event_cnt = 1;
      Particle.publish("EnvW/PIR", String(pir_event_cnt));
      #ifdef serial_debug
      Log.info("pir_cnt: %i", pir_event_cnt);
      #endif
    }
  }*/

  // ****************** light sensor ******************
  // get illuminance value if sensor is on
  if(tsl_on){
    // get raw data
    uint16_t _broadband, _ir;
    if (tsl.getData(_broadband,_ir,autoGainOn)){
      // now get the illuminance value
      tsl.getLux(integrationTime,_broadband,_ir,illuminance);
    }
    // problem with reading, flag and turn off sensor logical switch
    else {
      illuminance = -0.01;
      sprintf(sensors_status,"EnvW/tsl error:%i",tsl.getError());
      // sprintf(sensors_status,"EnvD/tsl error:%i",tsl.getError());
      tsl.setPowerDown();
      tsl_on = false;
    }
    // print current settings to cloud variable
    printTSLsettings(tsl_settings);
  }

  // ****************** Temperature + humidity sensor ******************
  // get T/H values if sensor is on
  if (ht_on){
    // get data
    double hum = si7021.getRH();
    double temp = si7021.readTemp();

    //check if there  was I2C comm errors
    if (hum == 0.01) {
      humidity =  0.01;
      temperatureC = 0.01;
      // display error code
      strcpy(sensors_status,"TH error");
      // turn off sensor logical switch
      ht_on = false;
    }
    else {
      humidity =  hum;
      temperatureC = temp;
    }
  }

  // ****************** battery voltage and state of charge sensor ******************
  // get battery related data. There's no fail recovery option for this sensor yet
  if (batt_on) {
    // get battery data
    voltage = lipo.getVoltage();
    battery_p = lipo.getSOC();
    if (battery_p > 100.0) {
      battery_p = 100.0;
    }
    // low_voltage_alert = lipo.getAlert();
    // check sensor status and update logical switch if it's down
    if (lipo.getVersion() != 3) {
      batt_on = false;
    }
  }

  // update sensors status
  sprintf(sensors_status,"Ill: %i, TH: %i, PIR: %i, Batt: %i",tsl_on, ht_on, pir_on, batt_on);
  // update measurements string
  sprintf(data_c, "T=%.1f C, RH=%.1f, Ill=%.1f lux, v=%.2f V, soc=%.1f",temperatureC,humidity,illuminance,voltage,battery_p);

  #ifdef serial_debug
  Log.info(sensors_status);
  Log.info(data_c);
  #endif
}

// ****************** sensors Initialization functions ******************

bool enableTSL(){

  // (re-)start Wire periph and check sensor is connected
  if(!tsl.begin()){
    sprintf(sensors_status,"tsl error1:%i",tsl.getError());
    return false;
  }

  // x1 gain, 101ms integration time
  if(!tsl.setTiming(false,1,integrationTime))
  {
    sprintf(sensors_status,"tsl error2:%i",tsl.getError());
    return false;
  }

  if (!tsl.setPowerUp()){
    sprintf(sensors_status,"tsl error3:%i",tsl.getError());
    return false;
  }

  // enable auto gain by default
  autoGainOn = true;
  // done with init.
  return true;
}

bool enableBatt() {
  lipo.begin();
  lipo.quickStart();
  lipo.setThreshold(10);

  if (lipo.getVoltage()){
    return true;
  }
  else {
    sprintf(sensors_status,"batt Begin:%i",0);
    return false;
  }
}

bool enablePir() {
  pinMode(pirDetectPin, INPUT); // high on detection
  pinMode(pirEnablePin, OUTPUT); // pir on/off, high to enable

  // power cycle the sensor
  digitalWrite(pirEnablePin, LOW);
  delay(100);
  digitalWrite(pirEnablePin, HIGH);
  delay(500);

  #ifdef serial_debug
  Log.info("pirOutPin: %i", digitalRead(pirDetectPin));
  #endif

  if (digitalRead(pirDetectPin) == HIGH) {
    // sensor detected, calibration is on now
    pir_calibration_on = true;
    Particle.publish("EnvW/PIR","Start Cal");
    // Particle.publish("EnvD/PIR","Start Cal");
    return true;
  } else {
    return false;
  }
}

void printTSLsettings(char *buffer)
{
  if (!tsl._gain) {
    sprintf(buffer,"G:x1, IT: %i ms, %i",integrationTime,autoGainOn);
  }
  else if (tsl._gain) {
    sprintf(buffer,"G:x16, IT: %i ms,%i",integrationTime,autoGainOn);
  }
}

/* **********************************************************
*              Mode control Functions
*  ********************************************************** */

void buttonHandler(system_event_t event, int data) {
	//int times = system_button_clicks(data);//put here for reference
	save_mode_on = !save_mode_on;
  save_mode_switched = true;
  // turn on Blue LED on D7 when save_mode_switched is set
  digitalWrite(D7, HIGH);
}

void blink_on_switch() {
	// visual indication on tracking state transition
	RGB.control(true);
  if (save_mode_on) RGB.color(51, 255,  51); //lime
  else RGB.color(255, 49,  0); //orange
	delay(100);
	RGB.color(0, 0,  0);
	delay(70);
  if (save_mode_on) RGB.color(51, 255,  51); // lime
  else RGB.color(255, 49,  0); //orange
  delay(100);
	RGB.control(false);
}

/* **********************************************************
*              Settings Cloud Functions
*  ********************************************************** */

int resetSensors(String command){

  const char *command_c = command.c_str();
  uint8_t exit_code = -1;

  if (strcmp(command_c, "th")==0 || strcmp(command_c, "ht")==0 || strcmp(command_c, "all")==0 ) {
    si7021.reset();
    ht_on = si7021.begin();
    exit_code = 0;
  }
  if (strcmp(command_c, "i")==0 || strcmp(command_c, "all")==0 ) {
    tsl_on = enableTSL();
    exit_code = 0;
  }

  if (strcmp(command_c, "b")==0 || strcmp(command_c, "all")==0 ) {
    lipo.reset();
    batt_on = enableBatt();
    exit_code = 0;
  }

  // update status
  sprintf(sensors_status,"Ill: %i, TH: %i, PIR: %i, Batt: %i",tsl_on, ht_on, pir_on, batt_on);

  return exit_code;
}

// cloud function to change exposure settings (gain and integration time)
int setExposure(String command)
{
  //command is expected to be "gain={0,1,2},integrationTimeSwitch={0,1,2}"
  // gain = 0:x1, 1: x16, 2: auto
  // integrationTimeSwitch: 0: 14ms, 1: 101ms, 2:402ms

  // private vars
  char gainInput;
  uint8_t itSwitchInput;
  boolean setTimingReturn = false;

  // extract gain as char and integrationTime swithc as byte
  gainInput = command.charAt(0);//we expect 0, 1 or 2
  itSwitchInput = command.charAt(2) - '0';//we expect 0,1 or 2

  if (itSwitchInput >= 0 && itSwitchInput < 3){
    // acceptable integration time value, now check gain value
    if (gainInput=='0'){
      setTimingReturn = tsl.setTiming(false,itSwitchInput,integrationTime);
      autoGainOn = false;
    }
    else if (gainInput=='1') {
      setTimingReturn = tsl.setTiming(true,itSwitchInput,integrationTime);
      autoGainOn = false;
    }
    else if (gainInput=='2') {
      autoGainOn = true;
      // when auto gain is enabled, set starting gain to x16
      setTimingReturn = tsl.setTiming(true,itSwitchInput,integrationTime);
    }
    else{
      // no valid settings, raise error flag
      setTimingReturn = false;
    }
  }
  else{
    setTimingReturn = false;
  }

  // setTiming has an error
  if(!setTimingReturn){
    //disable getting illuminance value
    tsl_on = false;
    return -1;
  }
  else {
    // all is good
    tsl_on = true;
    return 0;
  }
}

//cloud function to control whee data is sent
int setActiveCloud(String command)
// valid values: "ubidots", "particle", "influx", "all" or any other value for no cloud service
{
  // valid values: "ubidots", "particle", "influx", "all" or any other value for no cloud service
  const char *command_c = command.c_str();

  #ifdef serial_debug
  Log.info(command_c);
  #endif

  if (strcmp(command_c,"particle") == 0) { particle_on = true;}
  else if (strcmp(command_c,"ubidots") == 0) { ubidots_on = true;}
  else if (strcmp(command_c,"influx") == 0) { influxdb_on = true;}
  else if (strcmp(command_c,"all") == 0) {
    particle_on = true;
    ubidots_on = true;
    influxdb_on = true;
  }
  else if (strcmp(command_c,"none") == 0) {
    particle_on = false;
    ubidots_on = false;
    influxdb_on = false;
  }
  else {return -1; }// invalid command

  // there was sucessful change, update cloud services status
  sprintf(cloud_settings,"ubidots: %i, particle: %i, influx: %i", ubidots_on, particle_on, influxdb_on);

  #ifdef serial_debug
  Log.info(cloud_settings);
  #endif

  return 0;
}

// set tags for influxdb services
// set tags for influxdb
int setConditions(String command) {
  // check how many fields provided
  if (command.indexOf(",") == -1) {
    // no comma --> one value provided for location
    strcpy(location,command.c_str());
  }
  else {
    // split the command at the ,
    int split_index = command.indexOf(",");
    strcpy(location, command.substring(0, split_index));
    strcpy(humidifier, command.substring(split_index+1));
    }

    // if tags were modified, store the new values in emulated EEPROM
    TagsE3 stored_tags;
    EEPROM.get(0,stored_tags);

    #ifdef serial_debug
    Log.info("\nlocation:");
    Log.info(location);
    Log.info("\nhumidifier:");
    Log.info(humidifier);

    Log.info("location from E2:");
    Log.info(stored_tags.location);
    Log.info("humidifier from E2:");
    Log.info(stored_tags.humidifier);
    Log.info("version from E2: %i", stored_tags.version);

    #endif

    // compare to what was submitted and store if changed
    if (strcmp(location, stored_tags.location) != 0) {
      // location tag has changed, write to EEPROM
      strcpy(stored_tags.location, location);
      // increment # edits counter
      stored_tags.version++;
      EEPROM.put(0,stored_tags);
    }

    if (strcmp(humidifier, stored_tags.humidifier) != 0) {
      // location tag has changed, write to EEPROM
      strcpy(stored_tags.humidifier, humidifier);
      // increment # edits counter
      stored_tags.version += 1;
      EEPROM.put(0,stored_tags);
    }

    return stored_tags.version;
}

// get current tags for influxdb
int getConditions(String command) {
  String tags = String::format("location: %s, humidifier: %s", location, humidifier);
  Particle.publish("EnvW/tags", tags);
  return 0;
}


// set the energy saving mode (one way: once in, can't remotely get out of it)
int setEnergySaving(String command) {
  if (!save_mode_on) {
    save_mode_on = true;
    save_mode_switched = true;
    blink_on_switch();
    return 1;
  }
  else return 0;
}

/* **********************************************************
*              Send data Cloud Functions
*  ********************************************************** */
void sendInfluxdb() {

  // formatted statement needs to look like this:
  // var_name,tag1=tval1,tag2=tval2 value=value

  if (ht_on){
    influxdbRequest.body = String::format("temperature,loc=%s,humidifier=%s value=%.2f",location, humidifier, temperatureC);
    influxdbRequest.body.concat(String::format("\nhumidity,loc=%s,humidifier=%s value=%.1f",location, humidifier, humidity));
  }
  if (ht_on && tsl_on) {
    influxdbRequest.body.concat(String("\n"));
  }
  if(tsl_on) {
    influxdbRequest.body.concat(String::format("illuminance,loc=%s value=%.1f",location, illuminance));
  }
  if (pir_event_cnt > 0) {
    influxdbRequest.body.concat(String::format("\npresence,loc=%s value=%d", location, pir_event_cnt));
    // reset pir events counter
    pir_event_cnt = 0;
  }
  if(batt_on) {
    influxdbRequest.body.concat(String::format("\nvoltage value=%.2f", voltage));
    influxdbRequest.body.concat(String::format("\nbattery_p value=%.2f", battery_p));
  }

  //reset response content
  response.body = String("");
  response.status=0;

  http.post(influxdbRequest, response, influxdb_H);

  meas_sent++;

  if (response.status==204){
    meas_successful++;
    sprintf(influx_cnt, "%d/%d",meas_successful,meas_sent);
  }

  // send server response it to
  /*
  #ifdef serial_debug
    // let's see this hippo
    Log.info("request data...");
    Log.info("header0: %s", influxdb_H[0]);
    Log.info("header1: %s", influxdb_H[1]);
    Log.info("request: %s", influxdbRequest.body.c_str());
    Log.info("response msg: %s", response.body.c_str());
    Log.info("response status: %i", response.status);

  #endif
  */

}
