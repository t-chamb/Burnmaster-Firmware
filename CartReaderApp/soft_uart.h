#ifndef SOFT_UART_H
#define SOFT_UART_H

#include <stdint.h>

// PA13 (SWDIO) and PA14 (SWCLK) pins
#define SOFT_UART_TX_PIN  GPIO_PIN_13
#define SOFT_UART_TX_PORT GPIOA
#define SOFT_UART_RX_PIN  GPIO_PIN_14
#define SOFT_UART_RX_PORT GPIOA

// Timing for 115200 baud on 72MHz system
#define UART_DELAY_US 8

void soft_uart_init(void);
void soft_uart_send_byte(uint8_t byte);
void soft_uart_send_string(const char* str);

#endif // SOFT_UART_H