#include "Monitor/Temp.h"


uint8_t Monitor::Temp::takeMeasurement() {
  Monitor::takeMeasurement();

  float r_thermistor = r_fixed * ((1024.0/_raw_value) - 1);
  uint8_t idx = 0;

  while (idx < range_length) {
    if (r_thermistor <= r_range[idx]) {
      break;
    }
    idx++;
  }

  (r_range[idx] - r_thermistor)/(r_range[idx] - r_range[idx+1])
 
}

