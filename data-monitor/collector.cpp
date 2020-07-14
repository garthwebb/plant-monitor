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

#define SELF_RADIO_ADDR (uint8_t *) "1Node"
#define REMOTE_RADIO_ADDR (uint8_t *) "2Node"

#define COMMAND_STATUS 0x01

/****************** Raspberry Pi ***********************/

// Radio CE Pin, CSN Pin, SPI Speed
// See http://www.airspayce.com/mikem/bcm2835/group__constants.html#ga63c029bd6500167152db4e57736d0939 and the related enumerations for pin information.

// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 4Mhz
RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ);

/********************************/

int handleStatusCommand(uint32_t *payload) {
  uint16_t address;
  uint32_t cycles;
  uint32_t moisture;  

  // Read 2 bytes of address
  address = payload[1] << 8;
  address |= payload[2];

  // Read 3 bytes of cycles
  cycles = payload[3] << 16;
  cycles |= payload[4] << 8;
  cycles |= payload[5];

  // Read 3 byes of moisture
  moisture = payload[6] << 16;
  moisture |= payload[7] << 8;
  moisture |= payload[8];

  printf("Got payload:\n");
  printf("\taddress: %04x\n", address);
  printf("\tcycles: %d\n", cycles);
  printf("\tmoisture: %d\n", moisture);

  char curl_cmd[255];
  snprintf(curl_cmd, 255, "curl -i -XPOST 'http://192.168.1.79:8086/write?db=plants' --data-binary 'moisture,plant_id=%04x value=%d'", address, moisture);
  printf("%s\n", curl_cmd);
  system(curl_cmd);

  snprintf(curl_cmd, 255, "curl -i -XPOST 'http://192.168.1.79:8086/write?db=plants' --data-binary 'cycles,plant_id=%04x value=%d'", address, cycles);
  printf("%s\n", curl_cmd);
  system(curl_cmd);

  return 1;
}

int readCommand(void) {
  unsigned long result = 0;

  while (radio.available()) {
    uint8_t *command;
    command = (uint8_t *) malloc(9*sizeof(uint8_t));

    radio.read(command, 9*sizeof(uint8_t));

    if (command[0] == COMMAND_STATUS) {
        handleStatusCommand(command);
      result = 1;
    }

    free(command);
  }

  radio.stopListening();
  radio.write(&result, sizeof(unsigned long));
  radio.startListening();

  return 1;
}

void pongBack(void) {
  // Dump the payloads until we've gotten everything
  unsigned long got_time;

  // Fetch the payload, and see if this was the last one.
  while (radio.available()) {
    radio.read(&got_time, sizeof(unsigned long));
  }
  radio.stopListening();

  radio.write( &got_time, sizeof(unsigned long) );

  // Now, resume listening so we catch the next packets.
  radio.startListening();

  // Spew it
  printf("Got payload(%d) %lu...\n",sizeof(unsigned long), got_time);
}

int main(int argc, char** argv) {
  cout << "RF24/examples/GettingStarted/\n";

  // Setup and configure rf radio
  radio.begin();

  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15, 15);

  // Dump the configuration of the rf unit for debugging
  radio.printDetails();

  /***********************************/
  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.

  radio.openWritingPipe(REMOTE_RADIO_ADDR);
  radio.openReadingPipe(1, SELF_RADIO_ADDR);
	
  radio.startListening();
	
  while (1) {
    // Pong back role.  Receive each packet, dump it out, and send it back
			
    // if there is data ready
    if (radio.available()) {
      //pongBack();
      readCommand();

      //Delay after payload responded to, minimize RPi CPU time
      delay(925);
    }
  }

  return 0;
}

