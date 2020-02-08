
#include "stdio.h"
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "nrf24.h"
#include "radioPinFunctions.h"

uint8_t temp;
uint8_t q = 0;
uint8_t data_array[4];

//0x65646f4e32
// 1Node
uint8_t tx_address[5] = {0x31,0x4e,0x6f,0x64,0x65};
//uint8_t tx_address[5] = {0x65, 0x64, 0x6f, 0x4e, 0x31};

// 2Node
uint8_t rx_address[5] = {0x32,0x4e,0x6f,0x64,0x65};
//uint8_t rx_address[5] = {0x65, 0x64, 0x6f, 0x4e, 0x32};
/* ------------------------------------------------------------------------- */

#define OK_LED 5
#define ERR_LED 6

void setup_leds(void) {
    DDRB |= _BV(OK_LED) | _BV(ERR_LED);
}

void ok_led(void) {
    PORTB |= _BV(OK_LED);
    _delay_ms(300);
    PORTB &= ~_BV(OK_LED);
}

void err_led(void) {
    PORTB |= _BV(ERR_LED);
    _delay_ms(300);
    PORTB &= ~_BV(ERR_LED);
}

int main(void) {
    for (int x=0; x < 3; x ++) {
        ok_led();
        err_led();
    }

    /* init hardware pins */
    nrf24_init();
    
    /* Channel #2 , payload length: 4 */
    nrf24_config(2,4);

    /* Set the device addresses */
    nrf24_tx_address(tx_address);
    nrf24_rx_address(rx_address);    

    while (1) {
        /* Fill the data buffer */
        data_array[0] = 0x00;
        data_array[1] = 0xAA;
        data_array[2] = 0x55;
        data_array[3] = q++;                                    

        /* Automatically goes to TX mode */
        nrf24_send(data_array);        
        
        /* Wait for transmission to end */
        while (nrf24_isSending());

        /* Make analysis on last transmission attempt */
        temp = nrf24_lastMessageStatus();

        if (temp == NRF24_TRANSMISSON_OK) {
            //xprintf("> Tranmission went OK\r\n");
            ok_led();
        } else if (temp == NRF24_MESSAGE_LOST) {
            //xprintf("> Message is lost ...\r\n");
            err_led();
        }

		/* Retransmission count indicates the transmission quality */
		temp = nrf24_retransmissionCount();
		//xprintf("> Retransmission count: %d\r\n",temp);

		/* Optionally, go back to RX mode ... */
		nrf24_powerUpRx();

		/* Or you might want to power down after TX */
		// nrf24_powerDown();            

		/* Wait a little ... */
		_delay_ms(10);
    }
}
