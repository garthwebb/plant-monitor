/*
* Send moisture data from an Arduino Nano
*/
#include <SPI.h>
#include "RF24.h"

#define SELF_ID 0xbeef

#define HOST_RADIO_ADDR (byte *) "1Node"
#define SELF_RADIO_ADDR (byte *) "2Node"

#define RADIO_CSN 7
#define RADIO_CE 8

#define MOISTURE_ADC A4

#define COMMAND_STATUS 0x01

/****************** User Config ***************************/

// Hardware configuration: Set up nRF24L01 radio on SPI bus plus CE and CSN pins
RF24 radio(RADIO_CE, RADIO_CSN);

/**********************************************************/

uint32_t status_cycles = 0;

void setup() {
  Serial.begin(115200);
  Serial.println(F("Arduino Nano moisture send"));

  Serial.print(F("Long is: "));
  Serial.println(sizeof(unsigned long));

  radio.begin();
  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_LOW);
  //radio.setPALevel(RF24_PA_MAX);
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  radio.openWritingPipe(HOST_RADIO_ADDR);
  radio.openReadingPipe(1, SELF_RADIO_ADDR);
  
  // Start the radio listening for data
  radio.startListening();
}

uint8_t getMoistureValue(void) {
  return analogRead(MOISTURE_ADC);
}

int radioWriteBytes(uint8_t* buf, uint8_t len) {
  int success = 1;
  
  // First, stop listening so we can talk.
  radio.stopListening();         

  if (!radio.write(buf, len)) {
    success = 0;
  }

  // Now, continue listening
  radio.startListening();
  return success;
}

int radioWrite(unsigned long value) {
  Serial.println(F("Now sending"));
  int success = radioWriteBytes((uint8_t*) &value, sizeof(unsigned long));
  
  if (!success) {
    Serial.println(F("failed"));
  }

  return success;
}

int sendStatus(void) {
  radioWriteBytes(1234, 1);
  return;
  /*
  uint8_t *payload;
  uint8_t moisture_value = getMoistureValue();

  payload = malloc(9*sizeof(uint8_t));

  payload[0] = COMMAND_STATUS;

  payload[1] = (0xFF00 & SELF_ID) >> 8;
  payload[2] = 0x00FF & SELF_ID;

  payload[3] = (0x00FF0000 & status_cycles) >> 16;
  payload[4] = (0x0000FF00 & status_cycles) >> 8;
  payload[5] = 0x000000FF & status_cycles;
  
  payload[6] = (0x00FF0000 & moisture_value) >> 16;
  payload[7] = (0x0000FF00 & moisture_value) >> 8;
  payload[8] = 0x000000FF & moisture_value;

  if (!radioWriteBytes(payload, 9)) {
    Serial.println(F("failed to send payload"));
  }

  // Increment every time we send as a proxy for power
  status_cycles++;

  free(payload);
  */
}

void loop() {
  int moisture = getMoistureValue();

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
  unsigned long end_time = micros();

  // Describe the results
  Serial.print(F("Got moisture value = "));
  Serial.print(moisture);
  Serial.print(F(" after "));
  Serial.print(end_time-start_time);
  Serial.print(F(" us"));
  if (timeout) {
    Serial.println(F("Failed, response timed out."));
  } else {
    Serial.print(F(", Got response "));
    Serial.println(got_time);
  }
  
  // Try again 1m later
  delay(2*1000);
}
