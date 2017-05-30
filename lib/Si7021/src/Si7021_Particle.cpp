/*
	Si7021 Temperature and Humidity Sensor Library for Particle compatible products

	This is adapted from both Sparkfun's and Adafruit's libraries to make available
	a maximum features of the device.

	Jaafar Benabdallah
  February 2017

	Original development:

	 By: Joel Bartlett
	 SparkFun Electronics
	 Date: December 10, 2015

	 And:
	 By: Limor Fried
   Adafruit Industries

 Sensor datasheet:
 https://www.silabs.com/Support%20Documents%2FTechnicalDocs%2FSi7021-A20.pdf

 Tested on Photon and RedBear Duo with a Si7021 breakout from eBay.

 This Library is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This Library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 For a copy of the GNU General Public License, see
 <http://www.gnu.org/licenses/>.
 */

#include "Si7021_Particle.h"

 //Initialize
 Si7021::Si7021(){
   // init serial numbers variables
   sernum_a = sernum_b = 0;
 }

 bool Si7021::begin(void)
{
  Wire.begin();

  reset();
  delay(50);
  uint8_t ID_Temp_Hum = checkID();

  if (ID_Temp_Hum != 0x15) {
    return false;
  }

  if (readRegister8(READRHT_REG_CMD) != 0x3A) {
    Serial.print("reg_cmd_value:");
    Serial.println(READRHT_REG_CMD, HEX);
    return false;
  }

  // readSerialNumber();
  // Serial.println(sernum_a, HEX);
  // Serial.println(sernum_b, HEX);

  return true;
}

/****************Si7021 & HTU21D Functions**************************************/


float Si7021::getRH()
{
	// Measure the relative humidity
	uint16_t RH_Code = makeMeasurment(MEASRH_NOHOLD_CMD);
  if (RH_Code == 100) {
    return 0.01;
  }
	float result = (125.0*RH_Code/65536)-6;
	return result;
}

float Si7021::readTemp()
{
	// Read temperature from previous RH measurement.
	uint16_t temp_Code = makeMeasurment(READPREVTEMP_CMD);
  if (temp_Code == 100) {
    return 0.01;
  }
	float result = (175.25*temp_Code/65536)-46.85;
	return result;
}

float Si7021::getTemp()
{
	// Measure temperature
	uint16_t temp_Code = makeMeasurment(MEASTEMP_NOHOLD_CMD);
  if (temp_Code == 100) {
    return 0.01;
  }
	float result = (175.25*temp_Code/65536)-46.85;
	return result;
}
//Give me temperature in fahrenheit!
float Si7021::readTempF()
{
  return((readTemp() * 1.8) + 32.0); // Convert celsius to fahrenheit
}

float Si7021::getTempF()
{
  return((getTemp() * 1.8) + 32.0); // Convert celsius to fahrenheit
}

uint8_t Si7021::heaterOn(uint8_t setting)
{
  // write the heater current consumption value (datasheet rev 1.2, p. 27)
  // 0000 0000 -> 3.09mA
  // ...
  // 0000 0010 -> 15.24mA
  // ...
  // 0000 1111 -> 94.20mA
  setting &= 0x0F;
  writeRegister8(WRITEHEATER_REG_CMD, setting);

	// read user register and set heater bit
	uint8_t regVal = readRegister8(READRHT_REG_CMD);
	regVal |= _BV(HTRE);

	// turn on the heater
  writeRegister8(WRITERHT_REG_CMD, regVal);

  // return confirmation
  return readRegister8(READRHT_REG_CMD) & _BV(HTRE); //0x04
}

uint8_t Si7021::heaterOff()
{
	// read user register and reset heater bit
	uint8_t regVal = readRegister8(READRHT_REG_CMD);
	regVal &= ~_BV(HTRE);

  // turn off the heater
	writeRegister8(WRITERHT_REG_CMD, regVal);

  // return confirmation
  return readRegister8(READRHT_REG_CMD) & _BV(HTRE); //0x04
}

void Si7021::changeResolution(uint8_t i)
{
	// Changes to resolution of ADDRESS measurements.
	// Set i to:
	//      RH         Temp
	// 0: 12 bit       14 bit (default)
	// 1:  8 bit       12 bit
	// 2: 10 bit       13 bit
	// 3: 11 bit       11 bit

  // read current value of user register
	uint8_t regVal = readRegister8(READRHT_REG_CMD);

	// zero resolution bits
	regVal &= 0b011111110;

  // set resolution bits (b[7] and b[0])
	switch (i) {
	  case 1:
	    regVal |= 0b00000001;
	    break;
	  case 2:
	    regVal |= 0b10000000;
	    break;
	  case 3:
	    regVal |= 0b10000001;
	  default:
	    regVal |= 0b00000000;
	    break;
	}

	// write new resolution settings to the register
	writeRegister8(WRITERHT_REG_CMD, regVal);
}

void Si7021::reset()
{
	//Reset user resister
  writeRegister8(RESET_CMD);
}

uint8_t Si7021::checkID()
{
	uint8_t ID_1;

 	// Check device ID
	Wire.beginTransmission(ADDRESS);
	Wire.write(0xFC);
	Wire.write(0xC9);
	Wire.endTransmission();

    Wire.requestFrom(ADDRESS,1);

    ID_1 = Wire.read();

    return(ID_1);
}

uint16_t Si7021::makeMeasurment(uint8_t command)
{
	// Take one ADDRESS measurement given by command.
	// It can be either temperature or relative humidity

	uint16_t nBytes = 3;
	// if we are only reading old temperature, read olny msb and lsb
	if (command == 0xE0) nBytes = 2;

  writeRegister8(command);

	// When not using clock stretching (*_NOHOLD commands) delay here
	// is needed to wait for the measurement.
	// According to datasheet the max. conversion time is ~22ms
	delay(30);

	Wire.requestFrom(ADDRESS,nBytes);
	//Wait for data
	int counter = 0;
	while (Wire.available() < nBytes){
	  delay(1);
	  counter ++;
	  if (counter >100){
	    // Timeout: Sensor did not return any data
	    return 100;
	  }
	}

	unsigned int msb = Wire.read();
	unsigned int lsb = Wire.read();
	// Clear the last to bits of LSB to 00.
	// According to datasheet LSB of RH is always xxxxxx10
	lsb &= 0xFC;
	unsigned int measurment = msb << 8 | lsb;

	return measurment;
}

/********************************************************************/
/*              Low Level Functions                                 */
/********************************************************************/

void Si7021::writeRegister8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADDRESS);
  Wire.write((uint8_t)reg);
  Wire.write((uint8_t)value);
  Wire.endTransmission();

  //Serial.print("Wrote $"); Serial.print(reg, HEX); Serial.print(": 0x"); Serial.println(value, HEX);
}

void Si7021::writeRegister8(uint8_t reg) {
  Wire.beginTransmission(ADDRESS);
  Wire.write((uint8_t)reg);
  Wire.endTransmission();
}

uint8_t Si7021::readRegister8(uint8_t reg) {
  uint8_t value;
  Wire.beginTransmission(ADDRESS);
  Wire.write((uint8_t)reg);
  Wire.endTransmission(false);

  Wire.requestFrom(ADDRESS, 1);
  value = Wire.read();

  //Serial.print("Read $"); Serial.print(reg, HEX); Serial.print(": 0x"); Serial.println(value, HEX);
  return value;
}

/*uint16_t Si7021::readRegister16(uint8_t reg) {
  uint16_t value;
  Wire.beginTransmission(ADDRESS);
  Wire.write((uint8_t)reg);
  Wire.endTransmission();

  Wire.requestFrom(ADDRESS, 2);
  value = Wire.read();
  value <<= 8;
  value |= Wire.read();

  //Serial.print("Read $"); Serial.print(reg, HEX); Serial.print(": 0x"); Serial.println(value, HEX);
  return value;
}*/

void Si7021::readSerialNumber(void) {
  Wire.beginTransmission(ADDRESS);
  Wire.write((uint8_t)ID1_CMD>>8);
  Wire.write((uint8_t)ID1_CMD&0xFF);
  Wire.endTransmission();

  Wire.requestFrom(ADDRESS, 8);
  sernum_a = Wire.read();
  Wire.read();
  sernum_a <<= 8;
  sernum_a |= Wire.read();
  Wire.read();
  sernum_a <<= 8;
  sernum_a |= Wire.read();
  Wire.read();
  sernum_a <<= 8;
  sernum_a |= Wire.read();
  Wire.read();

  Wire.beginTransmission(ADDRESS);
  Wire.write((uint8_t)ID2_CMD>>8);
  Wire.write((uint8_t)ID2_CMD&0xFF);
  Wire.endTransmission();

  Wire.requestFrom(ADDRESS, 8);
  sernum_b = Wire.read();
  Wire.read();
  sernum_b <<= 8;
  sernum_b |= Wire.read();
  Wire.read();
  sernum_b <<= 8;
  sernum_b |= Wire.read();
  Wire.read();
  sernum_b <<= 8;
  sernum_b |= Wire.read();
  Wire.read();
}
