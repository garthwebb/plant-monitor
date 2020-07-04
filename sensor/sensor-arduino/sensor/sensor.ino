/*
    ATtiny24/44/84 Pin map with CE_PIN 8 and CSN_PIN 7
  Schematic provided and successfully tested by Carmine Pastore (https://github.com/Carminepz)
                                  +-\/-+
    nRF24L01  VCC, pin2 --- VCC  1|o   |14 GND --- nRF24L01  GND, pin1
                            PB0  2|    |13 AREF
                            PB1  3|    |12 PA1
                            PB3  4|    |11 PA2 --- nRF24L01   CE, pin3
                            PB2  5|    |10 PA3 --- nRF24L01  CSN, pin4
                            PA7  6|    |9  PA4 --- nRF24L01  SCK, pin5
    nRF24L01 MOSI, pin7 --- PA6  7|    |8  PA5 --- nRF24L01 MISO, pin6
                                  +----+
*/

//--------- Includes

#include <avr/sleep.h>
#include <avr/wdt.h>
#include "RF24.h"

//--------- Defines

#define SENSOR_ADC_PIN 1
#define SENSOR_POWER_PIN 9
#define START_UP_DELAY_MS 250

#define STATUS_LED_PIN 10

#define SETUP_BUTTON_PIN 8

// analogue ref = VCC, input channel = VREF
#define ADMUX_READ_INTERNAL 0x21
// analogue ref = VCC, input channel = ADC1
#define ADMUX_READ_ADC1 0x01

#define SELF_ID 0xfeed

#define HOST_RADIO_ADDR (byte *) "1Node"
#define SELF_RADIO_ADDR (byte *) "2Node"

// CE and CSN are configurable, specified values for ATtiny84 as connected above
#define RAIDIO_CE_PIN 2
#define RAIDIO_CSN_PIN 3

#define COMMAND_STATUS 0x01

// Constants for setting the watchdog prescalar for a particular timeout
#define WDT_16ms  0
#define WDT_32ms  1
#define WDT_64ms  2
#define WDT_128ms 3
#define WDT_250ms 4
#define WDT_500ms 5
#define WDT_1s    6
#define WDT_2s    7
#define WDT_4s    8
#define WDT_8s    9

// Sleep for 8 seconds at a time
//#define WDT_TIMEOUT WDT_8s
#define WDT_TIMEOUT WDT_2s

// With 8 seconds per sleep cycle, 38 cycles gives just over 5min
//#define SLEEP_CYCLES 38
#define SLEEP_CYCLES 1


#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define SENSOR_POWER_ON digitalWrite(SENSOR_POWER_PIN, HIGH)
#define SENSOR_POWER_OFF digitalWrite(SENSOR_POWER_PIN, LOW)

#define LED_ON digitalWrite(STATUS_LED_PIN, HIGH)
#define LED_OFF digitalWrite(STATUS_LED_PIN, LOW)

//--------- Globals

RF24 radio(RAIDIO_CE_PIN, RAIDIO_CSN_PIN);

uint32_t sleep_cycles = 0;
uint32_t status_cycles = 0;

volatile boolean reset_watchdog = true;

//--------- Functions

void setup() {
  pinMode(SENSOR_ADC_PIN, INPUT);
  pinMode(SETUP_BUTTON_PIN, INPUT);
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);

  // Use VCC as the analog referene (default, but let's just call it out)
  analogReference(DEFAULT);

  SENSOR_POWER_ON;
  
  initRadio();
  setup_watchdog(WDT_TIMEOUT);
}

void initRadio() {
  // Setup and configure rf radio
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  //radio.setDataRate(RF24_1MBPS);
  radio.setAutoAck(1);

  // Max delay between retries & number of retries
  radio.setRetries(15, 15);

  // Open a writing and reading pipe on each radio, with opposite addresses
  radio.openWritingPipe(HOST_RADIO_ADDR);
  radio.openReadingPipe(1, SELF_RADIO_ADDR);

  // Start listening
  radio.startListening();
}

void setup_watchdog(uint8_t level) {
  // Holds the Watchdog Timer Prescale Select bits, WDPx
  uint8_t prescalar = construct_prescalar(level);

  // Clear the Watchdog Reset Flag, WDRF
  MCUSR &= ~_BV(WDRF);

  // Start timed sequence via Watchdog Enable (WDE).  Any change also requires the
  // Watchdog Change Enable bit to be set (WDCE)
  WDTCSR |= _BV(WDCE) | _BV(WDE);

  // Set the new watchdog timeout value described by our prescalar, with WDCE to enable the change
  WDTCSR = _BV(WDCE) | prescalar;

  // Set Watchdog Timeout Interrupt Enable (WDIE) to start the timer
  WDTCSR |= _BV(WDIE);
}

uint8_t construct_prescalar(uint8_t level) {
   uint8_t prescalar = 0;

  // Cap values at 9
  if (level > 9 ) {
    level = 9;
  }

  // Any value over 8 needs to set WDP3 which is in a different place then the other WPDx bits
  if (level >= 8) {
    prescalar = _BV(WDP3);
  }
  // The remaining three bits line up with the bottom three bits of WDTCSR,
  // which are WPD2, WPD1, WPD0
  prescalar |= level & 7;

  return prescalar;
}

// Use avr registers rather than arduino analog read when we're
// reading from the internal voltage reference rather than a pin.
void avrReadAnalog() {
  // start conversion by writing 1 to ADSC
  sbi(ADCSRA, ADSC);

  // wait until ADSC is clear
  while ((ADCSRA & (1<<ADSC)) !=0);
}

uint16_t getAdcValue(void) {
  uint8_t high, low;

  // Read from analog twice (16.6.2: The first ADC conversion result after switching reference
  // voltage source may be inaccurate, and the user is advised to discard this result.
  avrReadAnalog();
  avrReadAnalog();
  
  // this order is needed to guarantee reading of ADC result correctly
  low  = ADCL;
  high = ADCH;

  return (high << 8) | (low);
}

// ASSUMPTION: ADC is enabled AND refernece is set to Vcc AND input is set to 1.1v internal
double getBatteryVoltage(void) {
  uint16_t adc_result = getAdcValue();

  // adc = 1024*vref/vcc, therefore vcc = 1024*vref/adc
  return (1024 * 1.1) / adc_result;
}

uint16_t getMoistureValue(unsigned long vcc_reading) {
  // Set the ADC to read from the 1.1V internal source initially  
  ADMUX = ADMUX_READ_ADC1;

  // Need a delay when switching sources
  delay(10);
  
  return analogRead(SENSOR_ADC_PIN);
}

int sendStatus(void) {
  double vcc_reading = getBatteryVoltage();
  
  uint32_t payload[4];
  payload[0] = COMMAND_STATUS;
  payload[1] = SELF_ID;
  payload[2] = status_cycles;
  payload[3] = (uint32_t) (vcc_reading * 1000);
  //payload[3]
  payload[4] = getMoistureValue(vcc_reading);

  bool success = true;
  
  radio.stopListening();
  if (!radio.write(&payload, sizeof(payload))) {
    success = false;
  }
  radio.startListening();

  // Increment every time we send as a proxy for power
  status_cycles++;
}

unsigned long waitForAck(void) {
  // Set up a timeout period, get the current microseconds
  unsigned long started_waiting_at = micros();
  // Set up a variable to indicate if a response was received or not
  boolean timeout = false;

  // While nothing is received
  while ( !radio.available() ) {
    // If waited longer than 200ms, indicate timeout and exit while loop
    if (micros() - started_waiting_at > 2000000) {
      timeout = true;
      break;
    }      
  }
  
  unsigned long ack = 0;
  if (!timeout) {
    // Grab the ack/nack
    radio.read(&ack, sizeof(unsigned long));
  }

  return ack;
}

// Put system into the sleep state. System wakes up when watchdog times out
void system_sleep() {
  SENSOR_POWER_OFF;
  radio.stopListening();
  radio.powerDown();

  // switch Analog to Digitalconverter OFF
  cbi(ADCSRA, ADEN);

  // sleep mode is set here
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // System sleeps here
  sleep_mode();

  // System continues execution here when watchdog timed out 
  sleep_disable();
}

void wake_system() {
  // Power on and startup.  The code following the delay takes
  // on order of a a few ms, so there's little use trying include that
  // time to reduce this delay.
  SENSOR_POWER_ON;

  // Start the radio
  radio.powerUp();

  // Switch Analog to Digitalconverter ON
  sbi(ADCSRA, ADEN);

  // Set the ADC to read from the 1.1V internal source initially  
  ADMUX = ADMUX_READ_INTERNAL;

  // Give everything time to settle in (caps charge in the sensor, ADC startup, etc).
  delay(START_UP_DELAY_MS);
}

// Watchdog Interrupt Service / is executed when watchdog timed out
ISR(WDT_vect) {
  reset_watchdog = true;
}

void loop(void) {
  unsigned long ack;
  unsigned long vcc_reading;

  if (reset_watchdog) {
    reset_watchdog = false;

    if (sleep_cycles <= 0) {
      wake_system();

      LED_ON;
      sendStatus();
      ack = waitForAck();
      sleep_cycles = SLEEP_CYCLES;
      LED_OFF;
    }

    system_sleep();
    sleep_cycles--;
  }
}
