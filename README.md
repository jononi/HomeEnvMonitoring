# Home-Env-Monitor

Connect different sensors to a Photon and log/visualize the values periodically to monitor an indoor environment.
Currently supported sensors:
- temperature + humidity
- illuminance
- movement
- battery charge

Hardware:
- Particle's Photon
- optional: battery shield (with MAX17041 state of charge sensor) for Photon (Sparkfun's or Particle's) + LiPo battery
- PIR sensor
- temperature and humidity sensor (Si7021, BME280 or HTU21D)
- ambient light sensor (TSL2561 or TSL2591)

Software for logging and visualization:
- InfluxDB
- Grafana
or
- Ubidots
(Note: I use a Raspberry Pi 3 as the local server running InfluxDB + Grafana)

Other features:
- add tags as meta information of the measurements conditions like the location (which room) and the use of a humidifier.
the meta-data can be remotely configured (using Particle cloud functions) and it's stored in emulated EEPROM on the Photon so it can be recovered in the event of a reset
- when data is captured and logged, the device is put in sleep mode to maximize battery life in the case of a portable usage.
the current configuration allows a cycle of up to 5 seconds to capture and log the data and a minimum of 25 seconds in sleep mode.

To be added:
- sound (using an amplified analog mic)
- ambient air pressure (if using BME280)
- support for using Particle firmware/cloud service on a Raspberry Pi
