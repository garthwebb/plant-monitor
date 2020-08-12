/**
 * Moisture monitor
 */

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <RF24/RF24.h>

using namespace std;

#define SELF_RADIO_ADDR (uint8_t *) "3Node"
#define REMOTE_RADIO_ADDR (uint8_t *) "4Node"

// Time in milliseconds to sleep between checking the radio
#define RADIO_CHECK_DELAY 1

/****************** Raspberry Pi ***********************/

// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 4Mhz
RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ);

/********************************/

void init(void) {
    cout << "RF24/examples/GettingStarted/\n";

    // Setup and configure rf radio
    radio.begin();

    //radio.setPALevel(RF24_PA_HIGH);
    radio.setPALevel(RF24_PA_LOW);

    // optionally, increase the delay between retries & # of retries
    radio.setRetries(15, 15);

    radio.setDataRate(RF24_2MBPS);
    //radio.setDataRate(RF24_250KBPS);

    // Dump the configuration of the rf unit for debugging
    radio.printDetails();

    /***********************************/
    // This simple sketch opens two pipes for these two nodes to communicate
    // back and forth.

    radio.openWritingPipe(REMOTE_RADIO_ADDR);
    radio.openReadingPipe(1, SELF_RADIO_ADDR);

    radio.startListening();
}

int readCommand(void) {
    uint32_t payload[3];

    while (radio.available()) {
        radio.read(&payload, sizeof(payload));

        printf("Got payload (id=%c, seq=%d): %d\n", 'A'+payload[0], payload[1], payload[2]);
        // Send back the id, and the value
        uint32_t reply[2] = {payload[0], payload[2]};

        radio.stopListening();
        radio.write(&reply, sizeof(reply));
        radio.startListening();
    }
    return 1;
}

int main(int argc, char** argv) {
    init();

    while (1) {
        // if there is data ready
        if (radio.available()) {
            readCommand();
        }

        //Delay after payload responded to, minimize RPi CPU time
        delay(RADIO_CHECK_DELAY);
    }

    return 0;
}

