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

#include <avr/sleep.h>
#include <avr/wdt.h>

#include "RF24.h"

/****************** User Config ***************************/
// Set this radio as radio number 0 or 1
bool radioNumber = 0;

#define MOISTURE_ADC 0

#define SELF_ID 0xfeed

#define HOST_RADIO_ADDR (byte *) "1Node"
#define SELF_RADIO_ADDR (byte *) "2Node"

// CE and CSN are configurable, specified values for ATtiny84 as connected above
#define RAIDIO_CE 2
#define RAIDIO_CSN 3

#define COMMAND_STATUS 0x01

RF24 radio(RAIDIO_CE, RAIDIO_CSN);

uint32_t status_cycles = 0;

void setup() {
  pinMode(MOISTURE_ADC, INPUT);

  //blinkLed();
  //blinkLed();
  
  initRadio();
}

/*
void blinkLed(void) {
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}
*/

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

uint8_t getMoistureValue(void) {
  return analogRead(MOISTURE_ADC);
}

int radioWriteBytes(uint8_t* buf, uint8_t len) {
  int success = 1;
  
  // First, stop listening so we can talk.
  radio.stopListening();         

  if (!radio.write(buf, len*sizeof(uint8_t))) {
    success = 0;
  }

  // Now, continue listening
  radio.startListening();
  return success;
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

void loop(void) {
  //blinkLed();
  unsigned long start_time = micros();

  sendStatus();

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
  
  unsigned long got_time = 0;
  if (!timeout) {
    // Grab the response, compare, and send to debugging spew
    radio.read( &got_time, sizeof(unsigned long) );
  }

  // Try again 1m later
  delay(1000);
}
