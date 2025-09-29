#pragma onece

#define ADDRLOW    GPIOD
#define ADDRHIGH   GPIOA
#define DATA       GPIOE
#define CTRL       GPIOB

#define RST        GPIO_PIN_7
#define AUDIO_IN   GPIO_PIN_8
#define CLK        GPIO_PIN_12
#define WR         GPIO_PIN_13
#define RD         GPIO_PIN_14
#define CS         GPIO_PIN_15


extern int sramBanks;
extern int romBanks;
extern word lastByte;

void TestMemGB(boolean bFast);
void gbFlashScreen();
void gbScreen();

// Helper functions for ROM operations
uint32_t gb_get_rom_size_bytes(uint8_t romSize);
uint16_t gb_get_rom_banks(uint8_t romSize);
const char* gb_get_rom_size_string(uint8_t romSize);

// Standard GB cart operations  
void gb_switch_bank(uint16_t bank);
void gb_reset_banks(void);