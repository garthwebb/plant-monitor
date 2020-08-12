/*
 * Plant Sensor
 * 
 * Remote data logging of moisture and temperature values.  Also includes battery level to signal
 * when a recharge is necessary
 * 
 */

//========= Includes

#include <avr/sleep.h>
#include <avr/wdt.h>
#include "RF24.h"
#include <Registry.h>

#if defined(__AVR_ATmega328P__)
#include <STDINOUT.h>
#endif

//========= Defines

//-----------------
// Hardware pins

#define TEMP_ADC_PIN A0
#define SENSOR_ADC_PIN 1
#define SENSOR_POWER_PIN 9

#if defined(__AVR_ATmega328P__)
#define STATUS_LED_PIN 2
#define SETUP_BUTTON_PIN 3
#else
#define STATUS_LED_PIN 10
#define SETUP_BUTTON_PIN 8
#endif

// CE and CSN are configurable, see https://github.com/Carminepz/attiny-nRF24L01
#if defined(__AVR_ATmega328P__)
#define RAIDIO_CE_PIN 8
#define RAIDIO_CSN_PIN 7
#else
#define RAIDIO_CE_PIN 2
#define RAIDIO_CSN_PIN 3
#endif

//-----------------
// ADC constants

// analogue ref = VCC, input channel = VREF
#define ADMUX_READ_INTERNAL 0x21

// The max possible value from the moisture sensor is 3.3 volts
#define MOISTURE_MAX_VAL 3.3l

// Top half resistance (in kOhms) of voltage divider thermistor is part of
#define R_CONSTANT 200
// Number of discrete values defined for the thermistor resistance from datasheet.
#define RANGE_LEN 34

//-----------------
// Radio constants

#define MESSAGE_ACK_TTL 250001
#define MAX_RETRIES 3
#define READ_PACKET_LEN 3
#define RESPONSE_PACKET_LEN 2

#define HOST_RADIO_ADDR (byte *) "3Node"
#define SELF_RADIO_ADDR (byte *) "4Node"

// From the class docs: https://maniacbug.github.io/RF24/classRF24.html#a4c6d3959c8320e64568395f4ef507aef
// In multiples of 250us, max is 15. 0 means 250us, 15 means 4000us.
#define PACKET_RETRY_DELAY 15
#define PACKET_RETRIES 15

//-----------------
// Communication constants

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

//-----------------
// Sleep constants

// How long to wait after wake up to make sure ADC and moisture sensor
// have had time to settle
#define START_UP_DELAY_MS 250

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
//#define WDT_TIMEOUT WDT_2s

// With 8 seconds per sleep cycle, 38 cycles gives just over 5min
//#define SLEEP_CYCLES 38
#define SLEEP_CYCLES 1

//-----------------
// Inline functions

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

// Used to keep track of how many times we've slept
uint32_t sleep_cycles = 0;

// Keep a count of messages. Used as a message/packet ID
uint32_t message_counter = 0;

volatile boolean reset_watchdog = true;

Registry registry;

static const int8_t temp_range[RANGE_LEN] = {
  -40, -35, -30, -25, -20, -15, -10, -5,  0,
    5,  10,  15,  20,  25,  30,  35,  40, 45,
   50,  55,  60,  65,  70,  75,  80,  85, 90,
   95, 100, 105, 110, 115, 120, 125};
static const float r_range[RANGE_LEN] = {
  4397.119, 3088.599, 2197.225, 1581.881, 1151.037, 846.579, 628.988, 471.632, 357.012,
   272.500,  209.710,  162.651,  127.080,  100.000,  79.222,  63.167,  50.677,  40.904, 
    33.195,   27.091,   22.224,   18.323,   15.184,  12.635,  10.566,   8.873,   7.481, 
     6.337,    5.384,    4.594,    3.934,    3.380,   2.916,   2.522};

//--------- Functions

void blink(uint8_t num) {
  for (int i=0; i < num; i++) {
    LED_ON;
    delay(100);
    LED_OFF;
    delay(100);
  }
}

void setup() {
  
#if defined(__AVR_ATmega328P__)
  Serial.begin(115200);
  STDIO.open (Serial);
  Serial.println(F("Sensor v2"));
  Serial.print(F("\tlocaltime: "));
  Serial.println(__TIME_UNIX__);
  Serial.print(F("\Message ACK TTL (ms): "));
  Serial.println(MESSAGE_ACK_TTL/1000);
#endif

  pinMode(TEMP_ADC_PIN, INPUT);
  pinMode(SENSOR_ADC_PIN, INPUT);
  pinMode(SETUP_BUTTON_PIN, INPUT);
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);

  SENSOR_POWER_ON;

  initRadio();
  initCollectorID();

#if defined(__AVR_ATmega328P__)
  radio.printDetails();
#endif

  setupWatchdog(WDT_TIMEOUT);

  randomSeed((unsigned long) registry.getSelfID());
}

void initCollectorID() {
  if (!registry.hasCollectorID()) {
    refreshCollectorID();  
  }
}

void refreshCollectorID() {
  uint32_t id = findClosestCollector();
  if (id) {
    registry.setCollectorID(id);
  }
}

void initRadio() {
  // Setup and configure rf radio
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  //radio.setPALevel(RF24_PA_HIGH);
  
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

void setupWatchdog(uint8_t level) {
  // Holds the Watchdog Timer Prescale Select bits, WDPx
  uint8_t prescalar = constructPrescalar(level);

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

uint8_t constructPrescalar(uint8_t level) {
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

uint16_t getMoistureValue(double vcc) {
  uint16_t sensor_reading = analogRead(SENSOR_ADC_PIN);

  // This adc value calculated against a known but varible vcc that we've just measured.  Adjust
  // it against the moisture max value that is contant.
  return sensor_reading * (vcc / MOISTURE_MAX_VAL);
}

uint16_t getTemperatureValue() {  
  uint16_t adc_reading = analogRead(TEMP_ADC_PIN);
  float r_temp = R_CONSTANT * (adc_reading/(1024.0-adc_reading));

  // Edge case; too hot to handle
  if (r_temp >= r_range[0]) {
    return temp_range[0];
  }
  // Edge case; too cold to hold
  if (r_temp < r_range[RANGE_LEN]) {
    return temp_range[RANGE_LEN];
  }

  // Since the resistor and temp values are 34 discrete points in a range,
  // use a linear fit to estimate the temperature value in between.

  uint8_t idx = findClosestRVal(r_temp);

  // From 0.0 to 1.0, how far r_temp is from the low range
  float factor = (r_temp - r_range[idx])/(r_range[idx-1] - r_range[idx]);
  int8_t temp_delta = temp_range[idx-1] - temp_range[idx];

  return (int) (temp_delta * factor) + temp_range[idx];
}

uint8_t findClosestRVal(float r_temp) {
  uint8_t idx = 0;
  while (idx < RANGE_LEN) {
    // The r_range array is ordered big to small, so search from the front
    // until we find the first value that is smaller or equal to ours.  Then we know our
    // range is between this and the previous value.
    if (r_temp >= r_range[idx]) {
      break;
    }
    idx++;
  }

  // Even though we handled the edge cases in the function above, still handle it here
  if (idx == RANGE_LEN) {
    return idx - 1;
  }
  return idx;
}

uint32_t findClosestCollector(void) {
  // Still need a payload to fill out the packet we send
  uint32_t payload[3] = {0, 0, 0};
  uint32_t result;

#if defined(__AVR_ATmega328P__)
  Serial.println(F("Sending Find Collector command"));
#endif

  if (sendMessage(COMMAND_FIND_COLLECTOR, payload, &result)) {
#if defined(__AVR_ATmega328P__)
  Serial.print(F("\tfound collector: "));
  Serial.println(result);
#endif
    return result;
  }

#if defined(__AVR_ATmega328P__)
  Serial.println(F("\tDid NOT find a collector"));
#endif
  // If we couldn't find a collector, invalidate this flag
  registry.clearCollectorID();

  return 0;
}

bool sendStatus(void) {
  double vcc_reading = getBatteryVoltage();
  uint32_t moisture_reading = getMoistureValue(vcc_reading);
  uint32_t temperature_reading = getTemperatureValue();
  uint32_t payload[3], result;

#if defined(__AVR_ATmega328P__)
  Serial.println(F("Sending Status command"));
#endif

  payload[0] = (uint32_t) (vcc_reading * 1000);
  payload[1] = moisture_reading;
  payload[2] = temperature_reading;

  // If sending the message is successful and we get a successful response back, return success for
  // this command 
  if (sendMessage(COMMAND_STATUS, payload, &result)) {
    return result == 1;
  }

  return false;
}

bool sendMessage(uint8_t cmd, uint32_t *data, uint32_t *response) {
  uint32_t payload[8];
  bool success = false;
  uint8_t retry_count = 0;

#if defined(__AVR_ATmega328P__)
  uint32_t id = registry.getSelfID();
  Serial.print(F("\tSelf ID: "));
  Serial.println(id);
  id = registry.getCollectorID();
  Serial.print(F("\tCollector ID: "));
  Serial.println(id);
#endif

  payload[IDX_CMD] = cmd;
  payload[IDX_SENSOR_ID] = registry.getSelfID();
  payload[IDX_COLLECTOR_ID] = registry.getCollectorID();
  payload[IDX_MESG_CNTR] = message_counter;
  payload[IDX_RETRY_CNTR] = retry_count;
  payload[IDX_DATA_1] = data[0];
  payload[IDX_DATA_2] = data[1];
  payload[IDX_DATA_3] = data[2];

  while (not success && retry_count <= MAX_RETRIES) {
    radio.stopListening();
    if (radio.write(&payload, sizeof(payload))) {
      radio.startListening();

      if (readResponse(response))  {
        success = true;
        break;
      }
    }

    // If we didn't get a response, or the write failed, increase the retry count and copy to
    // the payload for analytics at the server
    retry_count++;
#if defined(__AVR_ATmega328P__)
  Serial.print(F("\tRetry: "));
  Serial.println(retry_count);
#endif
    payload[4] = retry_count;

    delay(random(250, 1250));
  }

  // Increment every unique message sent
  message_counter++;

  return success;
}

uint8_t readResponse(uint32_t *value) {
  // Response is always the sensor ID and a value
  uint32_t response[2];

  // Set up a timeout period, get the current microseconds
  unsigned long started_micros = micros();

  // Loop until we get a valid response or hit the TTL
  while ((micros() - started_micros) < MESSAGE_ACK_TTL) {
    if (radio.available()) {
      // Read the message we got
      radio.read(&response, sizeof(response));

      // If its for us return success
      if (response[0] == registry.getSelfID()) {
        *value = response[1];
        return 1;
      }
    }
  }

#if defined(__AVR_ATmega328P__)
  Serial.print(F("\t\ttimeout"));
#endif

  // If we get here, we timed out trying to find a response meant for us.
  return 0;
}

// Put system into the sleep state. System wakes up when watchdog times out
void systemSleep() {
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

  // System continues execution here when watchdog times out 
  sleep_disable();
}

void wakeSystem() {
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

#if defined(__AVR_ATmega328P__)
  Serial.begin(115200);
  Serial.println(F("Waking up ..."));
#endif
}

// Watchdog Interrupt Service / is executed when watchdog times out
ISR(WDT_vect) {
  reset_watchdog = true;
}

void deepSleep(uint32_t cycles) {
  sleep_cycles = cycles;

  while (sleep_cycles) {
    systemSleep();
    sleep_cycles--;
  }
  wakeSystem();
}

void loop(void) {
  unsigned long ack;
  unsigned long vcc_reading;

  if (reset_watchdog) {
    reset_watchdog = false;

    if (sleep_cycles <= 0) {
      wakeSystem();

      LED_ON;
      if (registry.hasCollectorID()) {
        sendStatus();
      } else {
        refreshCollectorID();
      }

      sleep_cycles = SLEEP_CYCLES;
      LED_OFF;
    }

    systemSleep();
    sleep_cycles--;
  }
}
