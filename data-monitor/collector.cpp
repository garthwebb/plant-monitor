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

// Our (the collector) ID
#define SELF_ID 0x8080

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

#define INFLUX_HOST "192.168.1.79"
#define INFLUX_PORT 8086
#define INFLUX_DB_NAME "plants"

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

    printf("Handling command 'find collector'\n");

    reply(id, getSelfID());
}

void handleStatusCommand(uint32_t *payload) {
    uint32_t id, cycles, retries, vcc, moisture;

    printf("Handling command 'status'\n");

    id = payload[IDX_SENSOR_ID];
    cycles = payload[IDX_MESG_CNTR];
    retries = payload[IDX_RETRY_CNTR];
    vcc = payload[IDX_DATA_1];
    moisture = payload[IDX_DATA_2];

    printf("Got payload for cmd(%d):\n", COMMAND_STATUS);
    printf("\tsensor id: %04x\n", id);
    printf("\tretries: %d\n", retries);
    printf("\tvcc: %d\n", vcc);
    printf("\tmoisture: %d\n", moisture);

    char base_cmd[255];
    const char *base_tmpl = "curl -i -XPOST 'http://%s:%d/write?db=%s' --data-binary";
    snprintf(base_cmd, 255, base_tmpl, getInfluxHost(), getInfluxPort(), getInfluxDBName());

    char full_cmd[255];
    const char *full_tmpl = "%s '%s,plant_id=%04x value=%d'";

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "moisture", id, moisture);
    printf("%s\n", full_cmd);
    system(full_cmd);

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "cycles", id, cycles);
    printf("%s\n", full_cmd);
    system(full_cmd);

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "battery", id, vcc);
    printf("%s\n", full_cmd);
    system(full_cmd);

    snprintf(full_cmd, 255, full_tmpl, base_cmd, "retries", id, retries);
    printf("%s\n", full_cmd);
    system(full_cmd);

    reply(id, RESPONSE_SUCCESS);
}

int readCommand(void) {
    unsigned long result = 0;
    uint32_t payload[8];

    while (radio.available()) {
        radio.read(&payload, sizeof(payload));

        // Ignore any message that wasn't meant for us
        if (payload[IDX_COLLECTOR_ID] != SELF_ID) {
            printf("Skipping message not meant for us (ID:%d != our ID:%d)\n", payload[IDX_COLLECTOR_ID], SELF_ID);
            continue;
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

int main(int argc, char** argv) {
    cout << "RF24/examples/GettingStarted/\n";

    // Setup and configure rf radio
    radio.begin();

    // optionally, increase the delay between retries & # of retries
    radio.setRetries(15, 15);

    //radio.setDataRate(RF24_1MBPS);
    radio.setDataRate(RF24_250KBPS);

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
            readCommand();
        }

        //Delay after payload responded to, minimize RPi CPU time
        delay(925);
    }

    return 0;
}

