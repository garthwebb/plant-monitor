#include "Monitor.h"
#include "Arduino.h"

Monitor::Monitor(uint8_t adc_pin) {
  _adc_pin = adc_pin;
  pinMode(adc_pin, INPUT);
}

void Monitor::takeMeasurement() {
  _raw_value = analogRead(_adc_pin);
}

uint16_t Monitor::getValue() {
  return _raw_value;
}
