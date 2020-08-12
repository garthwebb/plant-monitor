#ifndef MONITOR_H_
#define MONITOR_H_

class Monitor {
  public:
    Monitor(uint8_t adc_pin);
    uint16_t getValue(void);
    void takeMeasurement(void);
  private:
    uint8_t _adc_pin;
    uint16_t _raw_value;
};

#endif
