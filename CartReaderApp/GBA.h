#ifndef _GBA_H_
#define _GBA_H_

#define ADDR_1    GPIOD
#define ADDR_2    GPIOA
#define ADDR_3    GPIOE
#define CTRLGBA   GPIOB

#define CS_SRAM    GPIO_PIN_7
#define GBA_WR     GPIO_PIN_13
#define GBA_RD     GPIO_PIN_14
#define CS_ROM     GPIO_PIN_15

void TestMemGBA(boolean bFast);
void gbaScreen();

// Helper functions for GBA ROM operations
uint32_t gba_get_rom_size(void);
const char* gba_get_save_type_string(uint8_t saveType);
uint32_t gba_get_save_size(uint8_t saveType);
void gba_set_address(uint32_t address);
void gba_reset(void);

// Standard address modes
void setAddrInMode(void);
void setAddrOutMode(void);

#endif