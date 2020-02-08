#include <avr/sleep.h>
#include <avr/wdt.h>

#define LED_PIN 0

#define adc_disable() (ADCSRA &= ~(1<<ADEN))

void setup() {
  pinMode(LED_PIN, OUTPUT);
  
  blinkLed();
  blinkLed();
}

void blinkLed(void) {
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}

void powerDown(void) {
  PRR |= _BV(PRTIM1) | _BV(PRADC);
}

void powerUp(void) {
  PRR = 0;// &= ~_BV(PRTIM1) & ~_BV(PRADC);
}

// Handle watchdog timer interrupt
ISR(WDT_vect) {
  wdt_disable();  // disable watchdog
}

void myWatchdogEnable(const byte interval) { 
  MCUSR = 0;                          // reset various flags
  WDTCSR |= 0b00011000;               // see docs, set WDCE, WDE
  WDTCSR =  0b01000000 | interval;    // set WDIE, and appropriate delay

  wdt_reset();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
  sleep_mode();            // now goes to Sleep and waits for the interrupt
}

void loop(void) {
  powerDown();
  myWatchdogEnable (0b100001);
  powerUp();
  blinkLed();
}
