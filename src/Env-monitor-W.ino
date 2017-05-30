/*
* Home environement monitoring and logging
* Env-monitor
Jaafar Ben-Abdallah / May 2017
*/

// imports, definitions and global variables go here:
#include "Env-monitor-W.h"

void setup()
{
  #ifdef serial_debug
  delay(3000);
  Serial.begin(9600);
  #endif

  // initialize sensors and get status

  // tsl_opsOn = enableTSL();
  batt_opsOn = enableBatt();
  ht_opsOn = si7021.begin();
  pir_opsOn = enablePir();

  // init pir events counter
  pir_event_cnt = 1;

  sprintf(sensors_status,"Ill: %i, TH: %i, PIR: %i, Batt: %i",tsl_opsOn, ht_opsOn, pir_opsOn, batt_opsOn);

  Particle.publish("EnvW/Status", sensors_status);

  sprintf(cloud_settings,"ubidots: %i, particle: %i, influx: %i", ubidotsOn, particleOn, influxdbOn);

  // cloud variables (max 20)
  // Particle.variable("illuminance", illuminance);
  Particle.variable("humidity", humidity);
  Particle.variable("temperature", temperatureC);
  // Particle.variable("voltage",voltage);
  Particle.variable("percentage",battery_p);
  Particle.variable("sen_status", sensors_status);
  // Particle.variable("tsl_settings", tsl_settings);
  Particle.variable("pir_status", pir_calibration_on);
  Particle.variable("pir_counter", pir_event_cnt);
  // Particle.variable("ubi_http_cnt",ubidots_cnt);
  Particle.variable("influx_cnt",influx_cnt);
  Particle.variable("cloud_status",cloud_settings);

  // function on the cloud: change light sensor exposure settings (max 12)
  Particle.function("resetSensors", resetSensors);
  Particle.function("setCloud", setActiveCloud);
  Particle.function("setMeta", setConditions);
  Particle.function("getMeta", getConditions);

  // ubidots connection params
  ubidotsRequest.hostname = "things.ubidots.com";
  ubidotsRequest.port = 80;
  ubidotsRequest.path = "/api/v1.6/collections/values";

  // ubidots connection params
  //influxdbRequest.ip = {192,168,1,100};
  influxdbRequest.hostname = "local-server-url";
  influxdbRequest.port = 8086;
  influxdbRequest.path = "/write?db=dbName&rp=retention-policy-name&precision=s&u=influxusername&p=influxpassword";

  // uncomment next line only when need to reset emulated EEPROM
  //EEPROM.clear();

  // read last used tags value from emulated EEPROM
  TagsE3 stored_tags;

  EEPROM.get(0, stored_tags);

  strcpy(location, stored_tags.location);
  strcpy(humidifier, stored_tags.humidifier);

  #ifdef serial_debug
  Serial.println("Initialization completed");
  Serial.println(sensors_status);
  Serial.printlnf("si7021 id: %d", si7021.checkID());
  Serial.printlnf("cell gauge id: %d", lipo.getVersion());

  // get local IP
  Serial.printlnf("local IP:");
  Serial.println(WiFi.localIP());

  // get mac address
  byte mac[6];
  WiFi.macAddress(mac);
  for (int i=0; i<6; i++) {
    if (i) Serial.print(":");
    Serial.print(mac[i], HEX);
  }
  Serial.println();

  Serial.printlnf("free mem= %i",System.freeMemory());

  Serial.print("location from E2:");
  Serial.println(stored_tags.location);
  Serial.print("humidifier from E2:");
  Serial.println(stored_tags.humidifier);
  Serial.print("version from E2:");
  Serial.println(stored_tags.version);

  #endif

  // start timers
  sendTime = millis();
  senseTime = millis();
  //
  sleepingDuration = minSleepingDuration;
  lastAwakeTime = millis();

}//setup


void loop()
{
  //if ((millis() - senseTime) > sensePeriod) {
  if (millis() - lastAwakeTime < maxAwakeDuration) {
    // get illuminance value if sensor is on
    if(tsl_opsOn){
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
        tsl.setPowerDown();
        tsl_opsOn = false;
      }
      // print current settings to cloud variable
      printTSLsettings(tsl_settings);
    }

    // get T/H values if sensor is operational
    if (ht_opsOn){
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
        ht_opsOn = false;
      }
      else {
        humidity =  hum;
        temperatureC = temp;
      }
    }

    // check if PIR calibration phase is over
    if (pir_calibration_on == true && digitalRead(pirDetectPin) == LOW) {
      // calibration is over, turn reset flag
      pir_calibration_on = false;
      Particle.publish("EnvW/PIR","End Cal");
      // Particle.publish("EnvD/PIR","End Cal");
    }

    // check PIR sensor output
    if (pir_opsOn && !pir_calibration_on) {
      if (digitalRead(pirDetectPin) == HIGH ){
        // if there was previous events (>0), increment
        // else just set it to 1
        if (pir_event_cnt > 0) pir_event_cnt ++;
        else pir_event_cnt =1;

        #ifdef serial_debug
        Serial.printlnf("pir_cnt: %i", pir_event_cnt);
        #endif
        // Particle.publish("EnvW/PIR", String(pir_event_cnt));
      }
    }

    // get battery related data. There's no fail recovery option for this sensor yet
    if (batt_opsOn) {
      // get battery data
      voltage = lipo.getVoltage();
      battery_p = lipo.getSOC();
      if (battery_p > 100.0) {
        battery_p = 100.0;
      }
      low_voltage_alert = lipo.getAlert();
      // check sensor status and update logical switch if it's down
      if (lipo.getVersion() != 3) {
        batt_opsOn = false;
      }
    }

    // update sensors status
    sprintf(sensors_status,"Ill: %i, TH: %i, PIR: %i, Batt: %i",tsl_opsOn, ht_opsOn, pir_opsOn, batt_opsOn);

    // update measurements string
    sprintf(data_c, "T=%.1f C, RH=%.1f, Ill=%.1f lux, v=%.2f V, soc=%.1f",temperatureC,humidity,illuminance,voltage,battery_p);

    #ifdef serial_debug
    Serial.println(sensors_status);
    Serial.println(data_c);
    #endif

    // reset send timer
    // senseTime = millis();

    // send data if cloud connection is established
    if (Particle.connected()) {

      if ((tsl_opsOn || ht_opsOn)) {
        if(particleOn) {
          Particle.publish("EnvW/data",data_c);
          delay(200); // give sometime for the data to be published
        }
        if (ubidotsOn) {
          sendUbidots2();
        }
        if (influxdbOn) {
          sendInfluxdb();
        }

        // if pir counts > 0 and it was already logged, reset it
        if (pir_event_cnt > 0) {
          pir_event_cnt = 0;
        }
      }
      // set sleepingDuration in the case of successful data sending: min sleeping duration + awake time not used(5s - awakeTime)
      sleepingDuration = minSleepingDuration + (millis() - lastAwakeTime);
      // make sure to not come back to this section and start sleep phase ASAP
      lastAwakeTime = millis() - maxAwakeDuration;
    } // logging data
  } // awake mode handling
  else {
    // exceeded maxAwakeDuration in awake time: sleepingDuration = 25s or sending happened before maxAwakeDuration: sleepingDuration >= 25s
    // start sleep phase (stop mode)
    uint32_t sleepStartTime = millis();
    System.sleep(pirDetectPin, RISING, sleepingDuration / 1000);

    // woke up: check what caused it
    if (millis() - sleepStartTime < sleepingDuration) {
      // awakening by PIR triggering --> increment PIR event counter
      pir_event_cnt ++;
      // update sleeping duration
      sleepingDuration = sleepingDuration - (millis() - sleepStartTime);
    }
    else {
      // awakening because exceeded sleepingDuration, reset AwakeTime and sleepingDuration
      sleepingDuration = minSleepingDuration;
      lastAwakeTime = millis();
    }
  } // sleep mode handling
} //loop


/* **********************************************************
*              Sensors Functions
*  ********************************************************** */


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
  Serial.printlnf("pirOutPin: %i", digitalRead(pirDetectPin));
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
*              Settings Cloud Functions
*  ********************************************************** */

int resetSensors(String command){

  const char *command_c = command.c_str();
  uint8_t exit_code = -1;

  if (strcmp(command_c, "th")==0 || strcmp(command_c, "ht")==0 || strcmp(command_c, "all")==0 ) {
    si7021.reset();
    ht_opsOn = si7021.begin();
    exit_code = 0;
  }
  if (strcmp(command_c, "i")==0 || strcmp(command_c, "all")==0 ) {
    tsl_opsOn = enableTSL();
    exit_code = 0;
  }

  if (strcmp(command_c, "b")==0 || strcmp(command_c, "all")==0 ) {
    lipo.reset();
    batt_opsOn = enableBatt();
    exit_code = 0;
  }

  // update status
  sprintf(sensors_status,"Ill: %i, TH: %i, PIR: %i, Batt: %i",tsl_opsOn, ht_opsOn, pir_opsOn, batt_opsOn);

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
    tsl_opsOn = false;
    return -1;
  }
  else {
    // all is good
    tsl_opsOn = true;
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
  Serial.println(command_c);
  #endif

  if (strcmp(command_c,"particle") == 0) { particleOn = true;}
  else if (strcmp(command_c,"ubidots") == 0) { ubidotsOn = true;}
  else if (strcmp(command_c,"influx") == 0) { influxdbOn = true;}
  else if (strcmp(command_c,"all") == 0) {
    particleOn = true;
    ubidotsOn = true;
    influxdbOn = true;
  }
  else if (strcmp(command_c,"none") == 0) {
    particleOn = false;
    ubidotsOn = false;
    influxdbOn = false;
  }
  else {return -1; }// invalid command

  // there was sucessful change, update cloud services status
  sprintf(cloud_settings,"ubidots: %i, particle: %i, influx: %i", ubidotsOn, particleOn, influxdbOn);

  #ifdef serial_debug
  Serial.println(cloud_settings);
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
    Serial.print("\nlocation:");
    Serial.println(location);
    Serial.print("\nhumidifier:");
    Serial.println(humidifier);

    Serial.print("location from E2:");
    Serial.println(stored_tags.location);
    Serial.print("humidifier from E2:");
    Serial.println(stored_tags.humidifier);
    Serial.print("version from E2:");
    Serial.println(stored_tags.version);

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

/* **********************************************************
*              Send data Cloud Functions
*  ********************************************************** */

void sendInfluxdb() {

  // formatted statement needs to look like this:
  // var_name,tag1=tval1,tag2=tval2 value=value

  if (ht_opsOn){
    influxdbRequest.body = String::format("temperature,loc=%s,humidifier=%s value=%.2f",location, humidifier, temperatureC);
    influxdbRequest.body.concat(String::format("\nhumidity,loc=%s,humidifier=%s value=%.1f",location, humidifier, humidity));
  }
  if (ht_opsOn && tsl_opsOn) {
    influxdbRequest.body.concat(String("\n"));
  }
  if(tsl_opsOn) {
    influxdbRequest.body.concat(String::format("illuminance,loc=%s value=%.1f",location, illuminance));
  }
  if (pir_event_cnt > 0) {
    influxdbRequest.body.concat(String::format("\npresence,loc=%s value=%d", location, pir_event_cnt));
    // reset pir events counter
    pir_event_cnt = 0;
  }
  if(batt_opsOn) {
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
  #ifdef serial_debug
    // let's see this hippo
    char header[128];
    Serial.println("request:");
    sprintf(header,"%s",influxdb_H[0]);
    Serial.println(header);
    sprintf(header,"%s",influxdb_H[1]);
    Serial.println(header);
    Serial.println(influxdbRequest.body);
    // let's see server response
    Serial.println("response:");
    Serial.println(response.body);
    Serial.println("status:");
    Serial.println(response.status);
  #endif
}

void sendUbidots2() {
  /* example with two variables:
  [{"variable": "aaaab5cce67625445fbbbb","value":26.3},
  {"variable": "aaaaa5ab987f455925bbbbb", "value":43.7}]
  */
  // build request's body
  ubidotsRequest.body = String("[");
  if (ht_opsOn){
    ubidotsRequest.body.concat(String::format("{\"variable\":\"%s\",\"value\":%.1f},",ubiTempVarId,temperatureC));
    ubidotsRequest.body.concat(String::format("{\"variable\":\"%s\",\"value\":%.1f}",ubiHumVarId,humidity));
  }
  if (ht_opsOn && tsl_opsOn) {
    ubidotsRequest.body.concat(String(","));
  }
  if(tsl_opsOn) {
    ubidotsRequest.body.concat(String::format("{\"variable\":\"%s\",\"value\":%.1f}",ubiIllVarId,illuminance));
  }
  if(batt_opsOn) {
    ubidotsRequest.body.concat(String::format(",{\"variable\":\"%s\",\"value\":%.2f},",ubiBattV,voltage));
    ubidotsRequest.body.concat(String::format("{\"variable\":\"%s\",\"value\":%.2f}",ubiBattP,battery_p));
  }
  ubidotsRequest.body.concat(String("]"));

  // execute the post request
  http.post(ubidotsRequest, response, ubiHeaders);
  dots_sent++;

  // get the server response code
  if (response.status==200){
    dots_successful++;
  }
  sprintf(ubidots_cnt, "%d/%d", dots_successful,dots_sent);

  // get detailed response
  #ifdef serial_debug
    // let's see this hippo
    char header[128];
    Serial.println("request:");
    sprintf(header,"%s",ubiHeaders[0]);
    Serial.println(header);
    sprintf(header,"%s",ubiHeaders[1]);
    Serial.println(header);
    Serial.println(ubidotsRequest.body);
    // let's see server response
    Serial.println("response:");
    Serial.println(response.body);
    Serial.println("status:");
    Serial.println(response.status);
  #endif
}
