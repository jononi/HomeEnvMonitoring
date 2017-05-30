/*
Particle Photon HTU21D Temperature / Humidity Sensor Library
By: Jaafar Ben-Abdallah July 2015, 
Added: getError() propagates the Wire error id to help troubleshooting a communication problem
Original development by: Romain MP
Licence: GPL v3
*/

#include "HTU21D.h"

HTU21D::HTU21D(){
}

bool HTU21D::begin(void)
{
	// Only join the I2C bus as master if needed
	if(! Wire.isEnabled())
		Wire.begin();

	// Reset the sensor
	if (reset()){
		// Read User register after reset to confirm sensor is OK
		return(read_user_register() == 0x2); // 0x2 is the default value of the user register
	}
	// error in reset or register checking
	return false;
}

bool HTU21D::reset(){
	Wire.beginTransmission(HTDU21D_ADDRESS);
	Wire.write(SOFT_RESET);
	_error = Wire.endTransmission();
	if (_error == 0){
		delay(20);
		return (true);
	}

	return false;
}

uint8_t HTU21D::getError(void)
	// If any library command fails, you can retrieve an extended
	// error code using this command. Errors are from the wire library:
	// 0 = Success
	// 1 = Data too long to fit in transmit buffer
	// 2 = Received NACK on transmit of address
	// 3 = Received NACK on transmit of data
	// 4 = Other error
	// 5 = reading time out error
	// 6 = CRC fail
{
	return(_error);
}

float HTU21D::readHumidity(){
	Wire.beginTransmission(HTDU21D_ADDRESS);
	Wire.write(TRIGGER_HUMD_MEASURE_NOHOLD);
	_error = Wire.endTransmission();

	// Wait for the sensor to measure
	delay(18); // 16ms measure time for 12bit measures

	Wire.requestFrom(HTDU21D_ADDRESS, 3);

	//Wait for data to become available
	int counter = 0;
	while(!Wire.available()){
		counter++;
		delay(1);
		if(counter > 100) {
			_error = 5;
			return HTU21D_I2C_TIMEOUT; //after 100ms consider I2C timeout
		}
	}

	uint16_t h = Wire.read();
	h <<= 8;
	h |= Wire.read();

	// CRC check
	uint8_t crc = Wire.read();
	if(checkCRC(h, crc) != 0) {
		_error = 6;
		return(HTU21D_BAD_CRC);
	}

	h &= 0xFFFC; // zero the status bits
	float hum = h;
	hum *= 125;
	hum /= 65536;
	hum -= 6;

	return hum;
}

float HTU21D::readTemperature(){
	Wire.beginTransmission(HTDU21D_ADDRESS);
	Wire.write(TRIGGER_TEMP_MEASURE_NOHOLD);
	_error = Wire.endTransmission();

	// Wait for the sensor to measure
	delay(55); // 50ms measure time for 14bit measures

	Wire.requestFrom(HTDU21D_ADDRESS, 3);

	//Wait for data to become available
	int counter = 0;
	while(!Wire.available()){
		counter++;
		delay(1);
		if(counter > 100) {
			_error = 5;
			return HTU21D_I2C_TIMEOUT; //after 100ms consider I2C timeout
		}
	}

	uint16_t t = Wire.read();
	t <<= 8;
	t |= Wire.read();

	// CRC check
	uint8_t crc = Wire.read();
	if( checkCRC(t, crc) != 0) {
		_error = 6;
		return(HTU21D_BAD_CRC);
	}
	t &= 0xFFFC; // zero the status bits
	float temp = t;
	temp *= 175.72;
	temp /= 65536;
	temp -= 46.85;

	return temp;
}

void HTU21D::setResolution(byte resolution)
{
  byte userRegister = read_user_register(); //Go get the current register state
  userRegister &= 0b01111110; //Turn off the resolution bits
  resolution &= 0b10000001; //Turn off all other bits but resolution bits
  userRegister |= resolution; //Mask in the requested resolution bits

  //Request a write to user register
  Wire.beginTransmission(HTDU21D_ADDRESS);
  Wire.write(WRITE_USER_REG); //Write to the user register
  Wire.write(userRegister); //Write the new resolution bits
  _error = Wire.endTransmission();
}

byte HTU21D::read_user_register()
{
	//Request the user register
	Wire.beginTransmission(HTDU21D_ADDRESS);
	Wire.write(READ_USER_REG);
	_error = Wire.endTransmission();

	//Read result
	Wire.requestFrom(HTDU21D_ADDRESS, 1);

	return(Wire.read());
}

byte HTU21D::checkCRC(uint16_t message, uint8_t crc){
	uint32_t reste = (uint32_t)message << 8;
	reste |= crc;

	uint32_t diviseur = (uint32_t)CRC_POLY;

	for(int i = 0; i<16; i++){
		if (reste & (uint32_t)1<<(23 - i) )
			reste ^= diviseur;

		diviseur >>= 1;
	}

	return (byte)reste;
}
