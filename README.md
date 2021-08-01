# plant-monitor

Code for the remote moisture sensing plant monitor with a moisture sensor as well as a temperature sensor, controlled by an AVR ATTiny device.  The plant monitor runs off a rechargable LiPo battery and wakes up every ~10 min to send moisture, temperature and battery level data back to a central receiver (a RaspberryPi).  The receiver is plugged in and connected to a WiFi network.  It logs each data event to InfluxDB, which could be hosted on the receiving RaspberryPi or somewhere else.
