/**
 * Moisture monitor
 */

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include "RF24/RF24.h"

using namespace std;

#define SELF_RADIO_ADDR (uint8_t *) "3Node"
#define REMOTE_RADIO_ADDR (uint8_t *) "4Node"

// Our (the collector) ID
#define SELF_ID 0x8080l

#define COMMAND_STATUS 0x01
#define COMMAND_FIND_COLLECTOR 0x02

#define IDX_CMD 0
#define IDX_SENSOR_ID 1
#define IDX_COLLECTOR_ID 2
#define IDX_MESG_CNTR 3
#define IDX_RETRY_CNTR 4
#define IDX_DATA_1 5
#define IDX_DATA_2 6
#define IDX_DATA_3 7

#define RESPONSE_SUCCESS 1
#define RESPONSE_FAIL 0

// InfluxDB

#define INFLUX_HOST "tiger-pi"
#define INFLUX_PORT 8086
#define INFLUX_DB_NAME "plants"

// Time in milliseconds to sleep between checking the radio
#define RADIO_CHECK_DELAY 100

/****************** Raspberry Pi ***********************/

// Radio CE Pin, CSN Pin, SPI Speed
// See http://www.airspayce.com/mikem/bcm2835/group__constants.html#ga63c029bd6500167152db4e57736d0939
// and the related enumerations for pin information.
//
// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 4Mhz
RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ);

/********************************/

int getSelfID(void) {
    return SELF_ID;
}

const char* getInfluxHost(void) {
    return INFLUX_HOST;
}

int getInfluxPort(void) {
    return INFLUX_PORT;
}

const char* getInfluxDBName(void) {
    return INFLUX_DB_NAME;
}

void reply(uint32_t sensor_id, uint32_t value) {
    uint32_t response[2] = {sensor_id, value};

    radio.stopListening();
    radio.write(response, sizeof(response));
    radio.startListening();
}

void handleFindCollectorCommand(uint32_t *payload) {
    uint32_t id = payload[IDX_SENSOR_ID];

    printf("Handling command 'find collector' for sensor id %08x\n", id);

    reply(id, getSelfID());
}

void handleStatusCommand(uint32_t *payload) {
    uint32_t id, cycles, retries, vcc, moisture, temperature;

    // Success for the sensor just means we got the message.  Reply quickly so that
    // it can go back to sleep
    id = payload[IDX_SENSOR_ID];
    reply(id, RESPONSE_SUCCESS);

    cycles = payload[IDX_MESG_CNTR];
    retries = payload[IDX_RETRY_CNTR];
    vcc = payload[IDX_DATA_1];
    moisture = payload[IDX_DATA_2];
    temperature = payload[IDX_DATA_3];

    printf("Status (%04x): r=%d, vcc=%4d, m=%3d, t=%2d\n", id, retries, vcc, moisture, temperature);

    char base_cmd[255];
    const char *base_tmpl = "curl -s -i -XPOST 'http://%s:%d/write?db=%s' --data-binary";
    snprintf(base_cmd, 255, base_tmpl, getInfluxHost(), getInfluxPort(), getInfluxDBName());

    char full_cmd[255];
    const char *full_tmpl = "%s '%s,plant_id=%04x value=%d' >> /dev/null";

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "temperature", id, temperature);
    //printf("%s\n", full_cmd);
    system(full_cmd);

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "moisture", id, moisture);
    //printf("%s\n", full_cmd);
    system(full_cmd);

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "cycles", id, cycles);
    //printf("%s\n", full_cmd);
    system(full_cmd);

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "battery", id, vcc);
    //printf("%s\n", full_cmd);
    system(full_cmd);

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "retries", id, retries);
    //printf("%s\n", full_cmd);
    system(full_cmd);
}

int readCommand(void) {
    unsigned long result = 0;
    uint32_t payload[8];

    while (radio.available()) {
        radio.read(&payload, sizeof(payload));

        // Ignore any message that wasn't meant for us, unless its to find a new collector to talk to
        if ((payload[IDX_CMD] != COMMAND_FIND_COLLECTOR) && (payload[IDX_COLLECTOR_ID] != SELF_ID)) {
            printf("Skipping message not meant for us (ID:%d != our ID:%lu)\n", payload[IDX_COLLECTOR_ID], SELF_ID);
            return 0;
        }

        switch (payload[IDX_CMD]) {
            case COMMAND_STATUS:
                handleStatusCommand(payload);
                break;
            case COMMAND_FIND_COLLECTOR:
                handleFindCollectorCommand(payload);
                break;
        }
    }

    radio.stopListening();
    radio.write(&result, sizeof(unsigned long));
    radio.startListening();

    return 1;
}

void initRadio(void) {
    // Setup and configure rf radio
    radio.begin();

    // optionally, increase the delay between retries & # of retries
    radio.setRetries(15, 15);

    radio.setDataRate(RF24_2MBPS);
    //radio.setDataRate(RF24_250KBPS);

    // Dump the configuration of the rf unit for debugging
    radio.printDetails();

    // This opens pipes for two nodes to communicate back and forth.
    radio.openWritingPipe(REMOTE_RADIO_ADDR);
    radio.openReadingPipe(1, SELF_RADIO_ADDR);

    radio.startListening();
}

int main(int argc, char** argv) {
    cout << "Collector starting up ...\n";

    initRadio();

    while (1) {
        // Pong back role.  Receive each packet, dump it out, and send it back

        // if there is data ready
        if (radio.available()) {
            readCommand();
        }

        //Delay after payload responded to, minimize RPi CPU time
        delay(RADIO_CHECK_DELAY);
    }

    return 0;
}

