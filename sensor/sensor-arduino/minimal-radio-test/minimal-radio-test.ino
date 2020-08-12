#include "RF24.h"

#if defined(__AVR_ATmega328P__)
#include <STDINOUT.h>
#endif

#if defined(__AVR_ATmega328P__)
#define STATUS_LED_PIN 2
#define RAIDIO_CE_PIN 8
#define RAIDIO_CSN_PIN 7
#else
#define STATUS_LED_PIN 10
#define RAIDIO_CE_PIN 2
#define RAIDIO_CSN_PIN 3
#endif


#define HOST_RADIO_ADDR (byte *) "3Node"
#define SELF_RADIO_ADDR (byte *) "4Node"

#define MESSAGE_ACK_TTL 1250000
#define PACKET_RETRY_DELAY 15
#define PACKET_RETRIES 15

#define LED_ON digitalWrite(STATUS_LED_PIN, HIGH)
#define LED_OFF digitalWrite(STATUS_LED_PIN, LOW)

RF24 radio(RAIDIO_CE_PIN, RAIDIO_CSN_PIN);

void setup() {  
  pinMode(STATUS_LED_PIN, OUTPUT);

  LED_ON;
  delay(1000);
  LED_OFF;
  delay(1000);
  LED_ON;
  delay(1000);
  LED_OFF;
  delay(1000);
  LED_ON;
  delay(1000);
  LED_OFF;
  delay(1000);
  
  initRadio();

  
#if defined(__AVR_ATmega328P__)
  Serial.begin(115200);
  STDIO.open (Serial);
  Serial.println(F("Radio Test"));
  radio.printDetails();
#endif

}

void initRadio() {
  // Setup and configure rf radio
  radio.begin();

  //radio.setPALevel(RF24_PA_HIGH);
  radio.setPALevel(RF24_PA_LOW);
  
  //radio.setDataRate(RF24_250KBPS);
  radio.setDataRate(RF24_2MBPS);
  radio.setAutoAck(1);

  // Max delay between retries & number of retries
  radio.setRetries(PACKET_RETRY_DELAY, PACKET_RETRIES);

  // Open a writing and reading pipe on each radio, with opposite addresses
  radio.openWritingPipe(HOST_RADIO_ADDR);
  radio.openReadingPipe(1, SELF_RADIO_ADDR);

  // Start listening
  radio.startListening();
}

uint32_t seq = 0;
uint32_t self = 1;
bool sendMessage(uint32_t message, uint32_t *response) {
  uint32_t payload[3] = {self, seq++, message};
  radio.stopListening();
  if (radio.write(&payload, sizeof(payload))) {
    radio.startListening();

    if (readResponse(response))  {
      return true;
    }
  }

  return false;
}

bool readResponse(uint32_t *value) {
  uint32_t reply[2];

  // Set up a timeout period, get the current microseconds
  unsigned long started_micros = micros();

  // Loop until we get a valid response or hit the TTL
  while ((micros() - started_micros) < MESSAGE_ACK_TTL) {
    if (radio.available()) {
      // Read the message we got
      radio.read(reply, sizeof(reply));

      if (reply[0] == self) {
        *value = reply[1];
        return true;
      }
    }
  }

  // If we get here, we timed out trying to find a response meant for us.
  return false;
}

uint32_t message = 10;
void loop() {
  uint32_t response = 0;

  LED_ON;
  sendMessage(message, &response);
  LED_OFF;
  message = response + 2;
  delay(10000);
}
