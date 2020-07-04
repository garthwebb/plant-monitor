/*
* Send moisture data from an Arduino Nano
*/
#include <SPI.h>
#include "RF24.h"

#define HOST_RADIO_ADDR (byte *) "2Node"
#define SELF_RADIO_ADDR (byte *) "1Node"

#define RADIO_CSN 7
#define RADIO_CE 8

/****************** User Config ***************************/

// Hardware configuration: Set up nRF24L01 radio on SPI bus plus CE and CSN pins
RF24 radio(RADIO_CE, RADIO_CSN);

/**********************************************************/

void setup() {
  Serial.begin(115200);
  Serial.println(F("Arduino Nano moisture receive"));

  radio.begin();
  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  //radio.setPALevel(RF24_PA_LOW);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  radio.openWritingPipe(HOST_RADIO_ADDR);
  radio.openReadingPipe(1, SELF_RADIO_ADDR);
  
  // Start the radio listening for data
  radio.startListening();

  int rate = radio.getDataRate();
  Serial.print(F("Rate is: "));
  Serial.println(rate);
}

int readStatus(void) {
  uint32_t payload[5];
  uint32_t cmd, id, cycles, vcc, moisture;
  radio.read(&payload, sizeof(payload));

  cmd = payload[0];
  id = payload[1];
  cycles = payload[2];
  vcc = payload[3];
  moisture = payload[4];

  Serial.print(F("Got command: "));
  Serial.println(cmd);
  Serial.print(F("    ID: "));
  Serial.println(id);
  Serial.print(F("    cycles: "));
  Serial.println(cycles);
  Serial.print(F("    vcc (mV): "));
  Serial.println(vcc);
  Serial.print(F("    moisture: "));
  Serial.println(moisture);
}

void loop() {
  if (radio.available()) {
    // While there is data ready
    while (radio.available()) {
      // Get the payload
      readStatus();
    }

    // First, stop listening so we can talk   
    radio.stopListening();
    // Send the final one back.
    radio.write(1, sizeof(unsigned long) );

    // Now, resume listening so we catch the next packets.     
    radio.startListening();
    Serial.println(F("Sent ACK"));
  }
}
