#ifndef BOARD_H
#define BOARD_H

#include <avr/io.h>


#define F_CPU 8000000UL /* 8MHz */


/* Heartbeat LED: PB0. */
#define LED_BLINK_DDR    DDRB
#define LED_BLINK_PORT   PORTB
#define LED_BLINK_PIN    PB0

/* USART0, xTP transport: RXD0 = PD0, TXD0 = PD1. */
#define UART_XTP_RX_DDR  DDRD
#define UART_XTP_RX_PORT PORTD
#define UART_XTP_RX_PIN  PD0
#define UART_XTP_TX_DDR  DDRD
#define UART_XTP_TX_PORT PORTD
#define UART_XTP_TX_PIN  PD1
#define UART_XTP_BAUD    9600UL

/* Hardware TWI: SCL = PC0, SDA = PC1. */
#define TWI_SCL_DDR       DDRC
#define TWI_SCL_PORT      PORTC
#define TWI_SCL_PIN       PC0
#define TWI_SDA_DDR       DDRC
#define TWI_SDA_PORT      PORTC
#define TWI_SDA_PIN       PC1
#define TWI_SCL_FREQ_HZ   100000UL

/* LCD Display */
#define LCD_RS_DDR    DDRA
#define LCD_RS_PORT   PORTA
#define LCD_RS_PIN    PA0
#define LCD_EN_DDR    DDRA
#define LCD_EN_PORT   PORTA 
#define LCD_EN_PIN    PA1
#define LCD_D4_DDR    DDRA
#define LCD_D4_PORT   PORTA
#define LCD_D4_PIN    PA4
#define LCD_D5_DDR    DDRA
#define LCD_D5_PORT   PORTA 
#define LCD_D5_PIN    PA5
#define LCD_D6_DDR    DDRA
#define LCD_D6_PORT   PORTA
#define LCD_D6_PIN    PA6
#define LCD_D7_DDR    DDRA
#define LCD_D7_PORT   PORTA
#define LCD_D7_PIN    PA7

#endif /* BOARD_H */