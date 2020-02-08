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

#define MOISTURE_ADC 0
#define MOISTURE_POWER 10
#define MOISTURE_STARTUP_MS 250

#define SELF_ID 0xfeed

#define HOST_RADIO_ADDR (byte *) "1Node"
#define SELF_RADIO_ADDR (byte *) "2Node"

// CE and CSN are configurable, specified values for ATtiny84 as connected above
#define RAIDIO_CE 2
#define RAIDIO_CSN 3

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
#define WDT_TIMEOUT WDT_8s
// With 8 seconds per sleep cycle, 38 cycles gives just over 5min
#define SLEEP_CYCLES 38

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define SENSOR_POWER_ON digitalWrite(MOISTURE_POWER, HIGH)
#define SENSOR_POWER_OFF digitalWrite(MOISTURE_POWER, LOW)


//--------- Globals

RF24 radio(RAIDIO_CE, RAIDIO_CSN);

uint32_t sleep_cycles = 0;
uint32_t status_cycles = 0;

volatile boolean reset_watchdog = true;

//--------- Functions

void setup() {
  pinMode(MOISTURE_ADC, INPUT);
  pinMode(MOISTURE_POWER, OUTPUT);

  SENSOR_POWER_ON;
  
  initRadio();
  setup_watchdog(WDT_TIMEOUT);
}

void initRadio() {
  // Setup and configure rf radio
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
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

uint16_t getMoistureValue(void) {
  return analogRead(MOISTURE_ADC);
}

int sendStatus(void) {
  uint32_t payload[4];
  payload[0] = COMMAND_STATUS;
  payload[1] = SELF_ID;
  payload[2] = status_cycles;
  payload[3] = getMoistureValue();

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

  // Power on and startup.  The code following the delay takes
  // on order of a a few ms, so there's little use trying include that
  // time to reduce this delay.
  SENSOR_POWER_ON;  
  delay(MOISTURE_STARTUP_MS);

  // Switch Analog to Digitalconverter ON
  sbi(ADCSRA, ADEN);

  radio.powerUp();
}

// Watchdog Interrupt Service / is executed when watchdog timed out
ISR(WDT_vect) {
  reset_watchdog = true;
}

void loop(void) {
  unsigned long ack;

  if (reset_watchdog) {
    reset_watchdog = false;

    if (sleep_cycles <= 0) {
      sendStatus();
      ack = waitForAck();
      sleep_cycles = SLEEP_CYCLES;
    }

    system_sleep();
    sleep_cycles--;
  }
}
