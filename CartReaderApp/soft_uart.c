#include "soft_uart.h"
#include "gd32f10x.h"

static void delay_us(uint32_t us) {
    // Simple delay loop calibrated for ~72MHz
    volatile uint32_t count = us * 12;
    while(count--) {
        __NOP();
    }
}

void soft_uart_init(void) {
    // Enable clocks
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_AF);
    
    // Disable SWD to free PA13/PA14
    gpio_pin_remap_config(GPIO_SWJ_DISABLE_REMAP, ENABLE);
    
    // Configure TX pin (PA13) as output push-pull
    gpio_init(SOFT_UART_TX_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, SOFT_UART_TX_PIN);
    
    // Configure RX pin (PA14) as input floating (not used yet)
    gpio_init(SOFT_UART_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, SOFT_UART_RX_PIN);
    
    // Set TX high (idle state)
    gpio_bit_set(SOFT_UART_TX_PORT, SOFT_UART_TX_PIN);
}

void soft_uart_send_byte(uint8_t byte) {
    // Start bit (low)
    gpio_bit_reset(SOFT_UART_TX_PORT, SOFT_UART_TX_PIN);
    delay_us(UART_DELAY_US);
    
    // Send 8 data bits, LSB first
    for(int i = 0; i < 8; i++) {
        if(byte & (1 << i)) {
            gpio_bit_set(SOFT_UART_TX_PORT, SOFT_UART_TX_PIN);
        } else {
            gpio_bit_reset(SOFT_UART_TX_PORT, SOFT_UART_TX_PIN);
        }
        delay_us(UART_DELAY_US);
    }
    
    // Stop bit (high)
    gpio_bit_set(SOFT_UART_TX_PORT, SOFT_UART_TX_PIN);
    delay_us(UART_DELAY_US);
}

void soft_uart_send_string(const char* str) {
    while(*str) {
        soft_uart_send_byte(*str++);
    }
}