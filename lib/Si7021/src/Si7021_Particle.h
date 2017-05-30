/*
	Si7021 Temperature and Humidity Sensor Library for Particle compatible products

	This is adapted from both Sparkfun's and Adafruit's libraries to make available
	a maximum features of the device.

	Jaafar Benabdalla, February 2017

	Original development:

	 By: Joel Bartlett
	 SparkFun Electronics
	 Date: December 10, 2015

	 And:
	 By: Limor Fried (Adafruit Industries)

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

#ifndef Si7021_Particle_h
#define Si7021_Particle_h

#include "application.h"

/**************** Si7021 Definitions ***************************/

#define ADDRESS      							0x40

#define MEASRH_HOLD_CMD           0xE5
#define MEASRH_NOHOLD_CMD         0xF5
#define MEASTEMP_HOLD_CMD         0xE3
#define MEASTEMP_NOHOLD_CMD       0xF3
#define READPREVTEMP_CMD          0xE0
#define RESET_CMD                 0xFE
#define WRITERHT_REG_CMD          0xE6
#define READRHT_REG_CMD           0xE7
#define WRITEHEATER_REG_CMD       0x51
#define READHEATER_REG_CMD        0x11
#define ID1_CMD                   0xFA0F
#define ID2_CMD                   0xFCC9
#define FIRMVERS_CMD              0x84B8

#define HTRE											0x02
#define _BV(bit) (1 << (bit))


// #define TEMP_MEASURE_HOLD  0xE3
// #define HUMD_MEASURE_HOLD  0xE5
// #define TEMP_MEASURE_NOHOLD  0xF3
// #define HUMD_MEASURE_NOHOLD  0xF5
// #define TEMP_PREV   0xE0
//
// #define WRITE_USER_REG  0xE6
// #define READ_USER_REG  0xE7
// #define SOFT_RESET  0xFE





/**************** Si7021 Class **************************************/
class Si7021
{
public:
	// Constructor
	Si7021();

	bool  begin();

	// Si7021 Public Functions
	float 	getRH();
	float 	readTemp();
	float 	getTemp();
	float 	readTempF();
	float 	getTempF();
	uint8_t heaterOn(uint8_t setting);
	uint8_t heaterOff();
	void  	changeResolution(uint8_t i);
	void  	reset();
	uint8_t	checkID();
	void 		readSerialNumber(void);

	uint32_t sernum_a, sernum_b;

private:
	//Si7021 Private Functions
	uint16_t makeMeasurment(uint8_t command);
	uint8_t readRegister8(uint8_t reg);
  // uint16_t readRegister16(uint8_t reg);
  void writeRegister8(uint8_t reg);
	void writeRegister8(uint8_t reg, uint8_t value);
};

#endif
