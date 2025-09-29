#include "Common.h"
#include "GB.h"
#include "GBA.h"
#include "GB_Flash.h"
#include "soft_uart.h"
#include <string.h>
#include <stdio.h>

// External functions from GB.c and GBA.c
extern byte readByte_GB(word myAddress);
extern void writeByte_GB(int myAddress, byte myData);
extern word readWord_GBA(unsigned long myAddress);
extern void writeWord_GBA(unsigned long myAddress, word myWord);
extern byte readByte_GBA(unsigned long myAddress);
extern void writeByte_GBA(unsigned long myAddress, byte myByte);
extern void OutAddrBus(int myAddress);
extern void delay_GB(void);

// Data direction macros from GB.c
#define dataOut_GB() GPIO_CTL1(DATA) = 0x33333333
#define dataIn_GB() GPIO_CTL1(DATA) = 0x44444444

// Progress callback function pointer for GB.c
static void (*gb_progress_callback)(const char*, int, int) = NULL;

// Forward declarations
void gb_flash_execute_commands_with_chip(const flash_command_t* commands, uint8_t count, 
                                        enum write_pin pin, uint32_t sector_addr,
                                        const flash_chip_info_t* chip);

// Standard command sets for different flash families

// AMD/Fujitsu standard commands (555/2AA)
static const flash_commands_t amd_std_commands = {
    .unlock1_addr = 0x555,
    .unlock1_data = 0xAA,
    .unlock2_addr = 0x2AA,
    .unlock2_data = 0x55,
    
    .id_entry = {
        {0x555, 0xAA},
        {0x2AA, 0x55},
        {0x555, 0x90}
    },
    .id_entry_count = 3,
    
    .chip_erase = {
        {0x555, 0xAA},
        {0x2AA, 0x55},
        {0x555, 0x80},
        {0x555, 0xAA},
        {0x2AA, 0x55},
        {0x555, 0x10}
    },
    .chip_erase_count = 6,
    
    .sector_erase = {
        {0x555, 0xAA},
        {0x2AA, 0x55},
        {0x555, 0x80},
        {0x555, 0xAA},
        {0x2AA, 0x55},
        {0x0000, 0x30}  // SA placeholder
    },
    .sector_erase_count = 6,
    
    .write_sequence = {
        {0x555, 0xAA},
        {0x2AA, 0x55},
        {0x555, 0xA0}
    },
    .write_sequence_count = 3,
    
    .reset_addr = 0x000,
    .reset_data = 0xF0
};

// AMD alternate commands (AAA/555)
static const flash_commands_t amd_alt_commands = {
    .unlock1_addr = 0xAAA,
    .unlock1_data = 0xAA,
    .unlock2_addr = 0x555,
    .unlock2_data = 0x55,
    
    .id_entry = {
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0xAAA, 0x90}
    },
    .id_entry_count = 3,
    
    .chip_erase = {
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0xAAA, 0x80},
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0xAAA, 0x10}
    },
    .chip_erase_count = 6,
    
    .sector_erase = {
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0xAAA, 0x80},
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0x0000, 0x30}  // SA placeholder
    },
    .sector_erase_count = 6,
    
    .write_sequence = {
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0xAAA, 0xA0}
    },
    .write_sequence_count = 3,
    
    .reset_addr = 0x000,
    .reset_data = 0xF0
};

// SST commands
static const flash_commands_t sst_commands = {
    .unlock1_addr = 0x5555,
    .unlock1_data = 0xAA,
    .unlock2_addr = 0x2AAA,
    .unlock2_data = 0x55,
    
    .id_entry = {
        {0x5555, 0xAA},
        {0x2AAA, 0x55},
        {0x5555, 0x90}
    },
    .id_entry_count = 3,
    
    .chip_erase = {
        {0x5555, 0xAA},
        {0x2AAA, 0x55},
        {0x5555, 0x80},
        {0x5555, 0xAA},
        {0x2AAA, 0x55},
        {0x5555, 0x10}
    },
    .chip_erase_count = 6,
    
    .sector_erase = {
        {0x5555, 0xAA},
        {0x2AAA, 0x55},
        {0x5555, 0x80},
        {0x5555, 0xAA},
        {0x2AAA, 0x55},
        {0x0000, 0x30}  // SA placeholder
    },
    .sector_erase_count = 6,
    
    .write_sequence = {
        {0x5555, 0xAA},
        {0x2AAA, 0x55},
        {0x5555, 0xA0}
    },
    .write_sequence_count = 3,
    
    .reset_addr = 0x000,
    .reset_data = 0xF0
};

// W29C020 / 29EE020 commands
static const flash_commands_t w29c020_commands = {
    .unlock1_addr = 0x5555,
    .unlock1_data = 0xAA,
    .unlock2_addr = 0x2AAA,
    .unlock2_data = 0x55,
    
    .id_entry = {
        {0x5555, 0xAA},
        {0x2AAA, 0x55},
        {0x5555, 0x90}  // Standard ID command
    },
    .id_entry_count = 3,
    
    .write_sequence = {
        {0x5555, 0xAA},
        {0x2AAA, 0x55},
        {0x5555, 0xA0}
    },
    .write_sequence_count = 3,
    
    .reset_addr = 0x5555,
    .reset_data = 0xF0
};

// MX29GL256EL special commands
static const flash_commands_t mx29gl_commands = {
    .unlock1_addr = 0xAAA,
    .unlock1_data = CMD_MX_UNLOCK1,  // 0xA9
    .unlock2_addr = 0x555,
    .unlock2_data = CMD_MX_UNLOCK2,  // 0x56
    
    .id_entry = {
        {0xAAA, CMD_MX_UNLOCK1},
        {0x555, CMD_MX_UNLOCK2},
        {0xAAA, 0x90}
    },
    .id_entry_count = 3,
    
    .chip_erase = {
        {0xAAA, CMD_MX_UNLOCK1},
        {0x555, CMD_MX_UNLOCK2},
        {0xAAA, 0x80},
        {0xAAA, CMD_MX_UNLOCK1},
        {0x555, CMD_MX_UNLOCK2},
        {0xAAA, 0x10}
    },
    .chip_erase_count = 6,
    
    .write_sequence = {
        {0xAAA, CMD_MX_UNLOCK1},
        {0x555, CMD_MX_UNLOCK2},
        {0xAAA, 0xA0}
    },
    .write_sequence_count = 3,
    
    .reset_addr = 0x000,
    .reset_data = 0xF0
};

// F3:C3 specific commands - try standard AMD commands for erase
// But keep modified codes for write based on FlashGBX
static const flash_commands_t f3c3_commands = {
    .unlock1_addr = 0xAAA,
    .unlock1_data = 0xAA,  // Standard AMD
    .unlock2_addr = 0x555,
    .unlock2_data = 0x55,  // Standard AMD
    
    .id_entry = {
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0xAAA, 0x90}
    },
    .id_entry_count = 3,
    
    .chip_erase = {
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0xAAA, 0x80},
        {0xAAA, 0xAA},
        {0x555, 0x55},
        {0xAAA, 0x10}
    },
    .chip_erase_count = 6,
    
    .write_sequence = {
        {0xAAA, 0xA9},  // Modified for write
        {0x555, 0x56},  // Modified for write
        {0xAAA, 0xA0}
    },
    .write_sequence_count = 3,
    
    .reset_addr = 0x000,
    .reset_data = 0xF0
};

// GBA flash chip database (simplified for this header)
static const flash_chip_info_t gba_flash_chips[] = {
    // Atmel
    {0x1F, 0x3D, "AT29LV512", 0x10000, 0x80, 128, FLASH_AT29LV, false, 3, WRITE_PIN_WR, 20000, 1000, 10, NULL, false},
    // SST
    {0xBF, 0xD4, "SST39VF512", 0x10000, 0x1000, 0, FLASH_39SF, false, 3, WRITE_PIN_WR, 40000, 1000, 20, &sst_commands, false},
    // Macronix
    {0xC2, 0x1C, "MX29L512", 0x10000, 0x10000, 0, FLASH_29F, false, 3, WRITE_PIN_WR, 40000, 3000, 200, &amd_alt_commands, false},
    {0xC2, 0x09, "MX29L010", 0x20000, 0x20000, 0, FLASH_29F, false, 3, WRITE_PIN_WR, 40000, 3000, 200, &amd_alt_commands, false},
    // Panasonic
    {0x32, 0x1B, "MN63F805MNP", 0x10000, 0x2000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 50000, 2000, 100, &amd_std_commands, false},
    // Sanyo
    {0x62, 0x13, "LE26FV10N1TS", 0x20000, 0x1000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 40000, 1000, 50, &amd_std_commands, false},
    {0x62, 0x12, "F0088H0", 0x10000, 0x4000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 50000, 2000, 100, &amd_std_commands, false},
    // Mitsubishi
    {0x1C, 0x02, "4000L0YBQ0", 0x80000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 60000, 3000, 200, &amd_std_commands, false},
    {0x1C, 0x16, "4400L0ZDQ0", 0x100000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 60000, 3000, 200, &amd_std_commands, false},
    // End marker
    {0, 0, NULL, 0, 0, 0, FLASH_UNKNOWN, false, 0, WRITE_PIN_WR, 0, 0, 0, NULL, false}
};

// GB flash chip database
static const flash_chip_info_t gb_flash_chips[] = {
    // AMD/Fujitsu - standard chips
    {0x01, 0xAD, "AM29F016B", 0x200000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 50000, 3000, 200, &amd_alt_commands, false},
    {0x04, 0xAD, "AM29F016D", 0x200000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 50000, 3000, 200, &amd_alt_commands, false},
    {0x01, 0x41, "AM29F032B", 0x400000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 50000, 3000, 200, &amd_alt_commands, false},
    {0x04, 0xD4, "MBM29F033C", 0x400000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 50000, 3000, 200, &amd_alt_commands, false},
    {0x01, 0xD5, "AM29F080B", 0x100000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 50000, 3000, 200, &amd_alt_commands, false},
    {0x04, 0xD5, "MBM29F080", 0x100000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 50000, 3000, 200, &amd_alt_commands, false},
    
    // Special chips with CFI that report F3:C3 - Clone chip with special unlock codes
    {0xF3, 0xC3, "Clone 4MB (F3C3) CFI", 0x400000, 0x10000, 0, FLASH_CFI, false, 5, WRITE_PIN_WR, 65535, 3000, 1000, &f3c3_commands, true},
    
    // FunnyPlaying MidnightTrace Flash Cart - insideGadgets 8MB (must be in MBC5 mode)
    {0x01, 0x01, "MidnightTrace 8MB", 0x800000, 0x10000, 0, FLASH_CFI, false, 5, WRITE_PIN_WR, 60000, 3000, 200, &amd_alt_commands, false},
    
    // SST chips
    {0xBF, 0x10, "SST39SF010", 0x20000, 0x1000, 0, FLASH_39SF, false, 5, WRITE_PIN_WR, 40000, 1000, 20, &sst_commands, false},
    {0xBF, 0xB5, "SST39SF010A", 0x20000, 0x1000, 0, FLASH_39SF, false, 5, WRITE_PIN_WR, 40000, 1000, 20, &sst_commands, false},
    {0xBF, 0xB6, "SST39SF020A", 0x40000, 0x1000, 0, FLASH_39SF, false, 5, WRITE_PIN_WR, 40000, 1000, 20, &sst_commands, false},
    {0xBF, 0xB7, "SST39SF040", 0x80000, 0x1000, 0, FLASH_39SF, false, 5, WRITE_PIN_WR, 40000, 1000, 20, &sst_commands, false},
    {0xBF, 0x04, "SST28LF040", 0x80000, 0x100, 0, FLASH_28LF040, false, 5, WRITE_PIN_WR, 100, 0, 20, NULL, false},
    
    // Sanyo
    {0x62, 0x04, "29EE020", 0x40000, 0x80, 0, FLASH_29F160, false, 5, WRITE_PIN_WR, 50000, 0, 100, &w29c020_commands, false},
    
    // Winbond
    {0xDA, 0x10, "W29C020", 0x40000, 0x100, 0, FLASH_W29C020, false, 5, WRITE_PIN_WR, 100, 0, 10, &w29c020_commands, false},
    
    // Intel
    {0x89, 0xB0, "28F004S5", 0x80000, 0x40000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 60000, 5000, 200, NULL, false},
    {0x89, 0xB4, "28F016S5", 0x200000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 60000, 5000, 200, NULL, false},
    {0x89, 0xB8, "28F032S3", 0x400000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 60000, 5000, 200, NULL, false},
    
    // Sharp
    {0xB0, 0x88, "LH28F016SUT", 0x200000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_WR, 60000, 5000, 200, NULL, false},
    
    // Atmel
    {0x1F, 0x6B, "AT29C040", 0x80000, 0x100, 256, FLASH_AT29LV, false, 5, WRITE_PIN_WR, 40000, 0, 10, NULL, false},
    
    // Clone chips that might use AUDIO pin - increase priority by moving to earlier in list
    {0x00, 0xC3, "Clone 4MB (00C3) AUDIO", 0x400000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_AUDIO, 50000, 3000, 200, &amd_std_commands, false},
    {0xF3, 0xC3, "Clone 4MB (F3C3) AUDIO", 0x400000, 0x10000, 0, FLASH_CFI, false, 5, WRITE_PIN_AUDIO, 65535, 5000, 1000, &f3c3_commands, true},
    {0x01, 0x01, "AMD Generic 8MB AUDIO", 0x800000, 0x10000, 0, FLASH_29F, false, 5, WRITE_PIN_AUDIO, 60000, 5000, 2000, &amd_alt_commands, true},
    
    // Macronix MX29GL256EL (special unlock)
    {0xC2, 0x7E, "MX29GL256EL", 0x2000000, 0x20000, 0, FLASH_MX29GL, false, 3, WRITE_PIN_WR, 65535, 3500, 60, &mx29gl_commands, true},
    {0xC2, 0x21, "MX29GL256EL-ALT", 0x2000000, 0x20000, 0, FLASH_MX29GL, false, 3, WRITE_PIN_WR, 65535, 3500, 60, &mx29gl_commands, true},
    
    // End marker
    {0, 0, NULL, 0, 0, 0, FLASH_UNKNOWN, false, 0, WRITE_PIN_WR, 0, 0, 0, NULL, false}
};

// Write pin types for writeByte_GB_WithPin
void writeByte_GB_WithPin(int address, byte data, enum write_pin pin) {
    OutAddrBus(address);
    GPIO_OCTL(DATA) = (GPIO_OCTL(DATA)&0xFFFF00FF) + ((data << 8) & 0xFF00);

    // Wait till output is stable
    delay_GB();

    if (pin == WRITE_PIN_WR) {
        // Toggle WR(PH5) pin
        gpio_bit_reset(CTRL, WR);
        delay_GB();
        gpio_bit_set(CTRL, WR);
    } else if (pin == WRITE_PIN_AUDIO) {
        // Toggle AUDIO_IN(PE3) pin - Audio In on GameBoy Player Port
        gpio_bit_reset(CTRL, AUDIO_IN);
        delay_GB();
        gpio_bit_set(CTRL, AUDIO_IN);
    }
}

// GB Flash identification
bool gb_flash_detect(uint8_t* mfg_id, uint8_t* dev_id) {
    return gb_flash_detect_with_pin(mfg_id, dev_id, WRITE_PIN_WR);
}

bool gb_flash_detect_with_pin(uint8_t* mfg_id, uint8_t* dev_id, enum write_pin pin) {
    dataOut_GB();
    
    // Send ID command sequence
    writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_UNLOCK1, pin);
    writeByte_GB_WithPin(CMD_ADDR_555, CMD_UNLOCK2, pin);
    writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_AUTOSELECT, pin);
    
    delay(10);
    dataIn_GB();
    
    // Read manufacturer and device IDs
    *mfg_id = readByte_GB(0);
    *dev_id = readByte_GB(1);
    
    // Reset to read mode
    dataOut_GB();
    writeByte_GB_WithPin(0, CMD_RESET, pin);
    delay(10);
    dataIn_GB();
    
    return (*mfg_id != 0xFF && *dev_id != 0xFF);
}

// Get chip info from database
const flash_chip_info_t* gb_flash_get_chip_info(uint8_t mfg_id, uint8_t dev_id) {
    for (int i = 0; gb_flash_chips[i].name != NULL; i++) {
        if (gb_flash_chips[i].manufacturer_id == mfg_id &&
            gb_flash_chips[i].device_id == dev_id) {
            return &gb_flash_chips[i];
        }
    }
    return NULL;
}

// Get GBA chip info from database
const flash_chip_info_t* gba_flash_get_chip_info(uint8_t mfg_id, uint8_t dev_id) {
    for (int i = 0; gba_flash_chips[i].name != NULL; i++) {
        if (gba_flash_chips[i].manufacturer_id == mfg_id &&
            gba_flash_chips[i].device_id == dev_id) {
            return &gba_flash_chips[i];
        }
    }
    return NULL;
}

// Detect and match chip with specified pin
const flash_chip_info_t* gb_flash_detect_and_match(enum write_pin pin) {
    uint8_t mfg_id, dev_id;
    
    if (!gb_flash_detect_with_pin(&mfg_id, &dev_id, pin)) {
        return NULL;
    }
    
    // First try exact match
    for (int i = 0; gb_flash_chips[i].name != NULL; i++) {
        if (gb_flash_chips[i].manufacturer_id == mfg_id &&
            gb_flash_chips[i].device_id == dev_id &&
            gb_flash_chips[i].write_pin == pin) {
            return &gb_flash_chips[i];
        }
    }
    
    // If no exact match with pin, try any match
    return gb_flash_get_chip_info(mfg_id, dev_id);
}

// Generic flash identification with CFI support
bool gb_flash_identify_chip(uint8_t* mfg_id, uint8_t* dev_id, enum flash_type* type, 
                           bool* x16_mode, bool* switch_bits, uint16_t* banks) {
    soft_uart_send_string("GB Flash: Starting chip identification\r\n");
    
    // Try standard ID first (WR pin)
    if (gb_flash_detect_with_pin(mfg_id, dev_id, WRITE_PIN_WR)) {
        soft_uart_send_string("GB Flash: Detected via WR pin\r\n");
        char msg[64];
        sprintf(msg, "GB Flash: ID = %02X:%02X\r\n", *mfg_id, *dev_id);
        soft_uart_send_string(msg);
        
        // Check if it's in our database
        const flash_chip_info_t* chip_info = gb_flash_get_chip_info(*mfg_id, *dev_id);
        
        if (chip_info) {
            *type = chip_info->type;
            *x16_mode = false;  // GB chips are typically x8
            *switch_bits = false;  // Standard chips don't swap bits
            *banks = chip_info->size / 0x4000;  // 16KB banks
            
            soft_uart_send_string("GB Flash: Found in database\r\n");
            return true;
        }
    }
    
    // Try AUDIO pin detection
    soft_uart_send_string("GB Flash: Trying AUDIO pin detection\r\n");
    if (gb_flash_detect_with_pin(mfg_id, dev_id, WRITE_PIN_AUDIO)) {
        char msg[64];
        sprintf(msg, "GB Flash: Detected via AUDIO pin - ID = %02X:%02X\r\n", *mfg_id, *dev_id);
        soft_uart_send_string(msg);
        
        const flash_chip_info_t* chip_info = gb_flash_get_chip_info(*mfg_id, *dev_id);
        if (chip_info && chip_info->write_pin == WRITE_PIN_AUDIO) {
            *type = chip_info->type;
            *x16_mode = false;
            *switch_bits = false;
            *banks = chip_info->size / 0x4000;
            return true;
        }
    }
    
    // Try CFI mode
    soft_uart_send_string("GB Flash: Attempting CFI identification\r\n");
    if (gb_flash_enter_cfi_mode(false, WRITE_PIN_WR)) {
        soft_uart_send_string("GB Flash: Entered CFI mode\r\n");
        
        // Read CFI data
        dataIn_GB();
        uint8_t cfi_sig[3];
        cfi_sig[0] = readByte_GB(0x10);
        cfi_sig[1] = readByte_GB(0x11);
        cfi_sig[2] = readByte_GB(0x12);
        
        if (cfi_sig[0] == 'Q' && cfi_sig[1] == 'R' && cfi_sig[2] == 'Y') {
            soft_uart_send_string("GB Flash: Valid CFI signature found\r\n");
            
            // Read device size
            uint32_t device_size = 1 << readByte_GB(0x27);
            *banks = device_size / 0x4000;
            
            // Read voltage
            uint8_t vcc_min = readByte_GB(0x1B);
            uint8_t vcc_max = readByte_GB(0x1C);
            
            char msg[64];
            sprintf(msg, "GB Flash: CFI Size = %lu bytes, Banks = %d\r\n", device_size, *banks);
            soft_uart_send_string(msg);
            
            // Exit CFI mode and get actual chip ID
            dataOut_GB();
            writeByte_GB_WithPin(0, 0xF0, WRITE_PIN_WR);
            delay(10);
            
            // Get chip ID in normal mode
            if (!gb_flash_detect_with_pin(mfg_id, dev_id, WRITE_PIN_WR)) {
                *mfg_id = 0xFF;
                *dev_id = 0xFF;
            }
            
            *type = FLASH_CFI;
            *x16_mode = false;
            *switch_bits = false;
            
            return true;
        }
        
        // Exit CFI mode if signature not found
        dataOut_GB();
        writeByte_GB_WithPin(0, 0xF0, WRITE_PIN_WR);
        delay(10);
    }
    
    soft_uart_send_string("GB Flash: Identification failed\r\n");
    return false;
}

// Reset flash chip
void gb_flash_reset(void) {
    gb_flash_reset_with_pin(WRITE_PIN_WR);
}

void gb_flash_reset_with_pin(enum write_pin pin) {
    dataOut_GB();
    writeByte_GB_WithPin(0, CMD_RESET, pin);
    delay(10);
}

// Enter CFI mode
bool gb_flash_enter_cfi_mode(bool x16_mode, enum write_pin pin) {
    soft_uart_send_string("GB Flash: Attempting to enter CFI mode\r\n");
    dataOut_GB();
    
    // Reset first
    writeByte_GB_WithPin(0, CMD_RESET, pin);
    delay(10);
    
    // Send CFI query command
    if (x16_mode) {
        writeByte_GB_WithPin(CMD_ADDR_555, CMD_CFI_QUERY, pin);
    } else {
        writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_CFI_QUERY, pin);
    }
    
    delay(10);
    return true;
}

// Exit CFI mode - now takes chip info
void gb_flash_exit_cfi_mode(const flash_chip_info_t* chip) {
    soft_uart_send_string("GB Flash: Exiting CFI mode\r\n");
    dataOut_GB();
    
    if (chip && chip->commands) {
        // Use chip-specific reset sequence
        char msg[64];
        sprintf(msg, "GB Flash: Using chip-specific reset: addr=0x%04X data=0x%02X\r\n", 
                chip->commands->reset_addr, chip->commands->reset_data);
        soft_uart_send_string(msg);
        writeByte_GB_WithPin(chip->commands->reset_addr, 
                            chip->commands->reset_data, 
                            chip->write_pin);
    } else {
        // Standard CFI exit
        soft_uart_send_string("GB Flash: Using standard CFI exit (0xF0 to addr 0)\r\n");
        writeByte_GB_WithPin(0, 0xF0, WRITE_PIN_WR);
    }
    
    delay(10);
    soft_uart_send_string("GB Flash: CFI mode exit complete\r\n");
}

// Poll status using DQ7 method
bool gb_flash_poll_status(uint16_t address, uint8_t expected_data, uint32_t timeout_us) {
    dataIn_GB();
    uint32_t loops = 0;
    uint32_t max_loops = timeout_us / 10;
    
    while ((readByte_GB(address) & 0x80) != (expected_data & 0x80)) {
        loops++;
        if (loops > max_loops) {
            return false;
        }
        delayMicroseconds(10);
    }
    
    return true;
}

// Wait for toggle bit to stop toggling
bool gb_flash_wait_toggle_bit(uint16_t address, uint32_t timeout_us) {
    dataIn_GB();
    uint32_t loops = 0;
    uint32_t max_loops = timeout_us / 10;
    
    uint8_t last_read = readByte_GB(address);
    
    while (loops < max_loops) {
        uint8_t current_read = readByte_GB(address);
        
        // Check if DQ6 stopped toggling
        if ((last_read & 0x40) == (current_read & 0x40)) {
            return true;
        }
        
        last_read = current_read;
        loops++;
        delayMicroseconds(10);
    }
    
    return false;
}

// Check DQ7 for completion
bool gb_flash_check_dq7(uint16_t address, uint8_t expected_data, uint32_t timeout_us) {
    return gb_flash_poll_status(address, expected_data, timeout_us);
}

// Write byte with compensation
void gb_flash_write_byte_compensated(uint16_t address, uint8_t data, bool x16_mode, bool switch_bits, enum write_pin pin) {
    uint8_t compensated_data = data;
    
    if (switch_bits) {
        compensated_data = (data & 0b11111100) | ((data << 1) & 0b10) | ((data >> 1) & 0b01);
    }
    
    uint16_t compensated_addr = x16_mode ? (address >> 1) : address;
    
    writeByte_GB_WithPin(compensated_addr, compensated_data, pin);
}

// Read byte with compensation
uint8_t gb_flash_read_byte_compensated(uint16_t address, bool x16_mode, bool switch_bits) {
    uint16_t compensated_addr = x16_mode ? (address >> 1) : address;
    uint8_t data = readByte_GB(compensated_addr);
    
    if (switch_bits) {
        return (data & 0b11111100) | ((data << 1) & 0b10) | ((data >> 1) & 0b01);
    }
    
    return data;
}


// Erase with progress
bool gb_flash_erase_with_progress(const flash_chip_info_t* chip, uint8_t rom_banks, 
                                 void (*progress_callback)(const char*, int, int)) {
    if (!chip) return false;
    
    gb_progress_callback = progress_callback;
    
    // For CFI chips, use chip erase instead of sector erase
    if (chip->type == FLASH_CFI) {
        soft_uart_send_string("GB Flash: Using chip erase for CFI chip\r\n");
        return gb_flash_erase_chip(chip);
    }
    
    // For other chips, do sector-by-sector erase
    if (progress_callback) {
        progress_callback("Erasing", 0, rom_banks * 4);  // 4 sectors per bank
    }
    
    int sector_count = 0;
    for (uint8_t bank = 0; bank < rom_banks; bank++) {
        gb_flash_switch_bank(bank);
        
        // Erase 4 sectors per bank (4KB each)
        for (uint16_t sector = 0; sector < 0x4000; sector += 0x1000) {
            uint16_t addr = (bank == 0) ? sector : (0x4000 + sector);
            
            if (!gb_flash_erase_sector_new(chip, bank, addr)) {
                return false;
            }
            
            sector_count++;
            if (progress_callback) {
                char status[32];
                sprintf(status, "Erasing sector %d/%d", sector_count, rom_banks * 4);
                progress_callback(status, sector_count, rom_banks * 4);
            }
            
            LED_RED_BLINK;  // Red for erasing
        }
    }
    
    return true;
}

// Write with progress
bool gb_flash_write_with_progress(const flash_chip_info_t* chip, uint8_t* buffer, uint32_t size,
                                 void (*progress_callback)(const char*, int, int)) {
    if (!chip || !buffer) return false;
    
    gb_progress_callback = progress_callback;
    
    uint32_t banks = size / 0x4000;
    
    if (progress_callback) {
        progress_callback("Writing", 0, banks);
    }
    
    for (uint32_t bank = 0; bank < banks; bank++) {
        uint16_t start_addr = (bank == 0) ? 0x0000 : 0x4000;
        uint16_t end_addr = (bank == 0) ? 0x3FFF : 0x7FFF;
        uint32_t buffer_offset = bank * 0x4000;
        
        gb_flash_switch_bank(bank);
        
        for (uint16_t addr = start_addr; addr <= end_addr; addr++) {
            // Calculate the correct buffer index
            uint32_t buffer_idx = buffer_offset + (addr - start_addr);
            if (!gb_flash_write_byte_new(chip, bank, addr, buffer[buffer_idx])) {
                return false;
            }
            
            // Update progress and LED more frequently
            if ((addr & 0x7F) == 0) {  // Every 128 bytes
                LED_BLUE_BLINK;  // Blue for writing
                
                // Update progress more frequently within bank
                if (progress_callback && (addr & 0x3FF) == 0) {  // Every 1KB
                    uint32_t bytes_done = (bank * 0x4000) + (addr - start_addr);
                    uint32_t total_bytes = banks * 0x4000;
                    int percentage = (bytes_done * 100) / total_bytes;
                    char status[64];
                    sprintf(status, "Writing bank %lu/%lu (%d%%)", bank + 1, banks, percentage);
                    progress_callback(status, bytes_done / 1024, total_bytes / 1024);
                }
            }
        }
        
        if (progress_callback) {
            char status[32];
            sprintf(status, "Writing bank %lu/%lu", bank + 1, banks);
            progress_callback(status, bank + 1, banks);
        }
    }
    
    return true;
}

// Verify with progress
bool gb_flash_verify_with_progress(uint8_t* buffer, uint32_t size,
                                  void (*progress_callback)(const char*, int, int)) {
    if (!buffer) return false;
    
    gb_progress_callback = progress_callback;
    
    uint32_t banks = size / 0x4000;
    uint32_t errors = 0;
    
    if (progress_callback) {
        progress_callback("Verifying", 0, banks);
    }
    
    dataIn_GB();
    
    for (uint32_t bank = 0; bank < banks; bank++) {
        gb_flash_switch_bank(bank);
        
        uint16_t start_addr = (bank == 0) ? 0x0000 : 0x4000;
        uint16_t end_addr = (bank == 0) ? 0x3FFF : 0x7FFF;
        uint32_t buffer_offset = bank * 0x4000;
        
        for (uint16_t addr = start_addr; addr <= end_addr; addr++) {
            uint8_t read_val = readByte_GB(addr);
            uint8_t expected = buffer[buffer_offset + addr];
            
            if (read_val != expected) {
                errors++;
                
                // Report first few errors
                if (errors <= 10) {
                    char msg[64];
                    sprintf(msg, "Verify error at bank %lu, addr 0x%04X: %02X != %02X\r\n",
                            bank, addr, read_val, expected);
                    soft_uart_send_string(msg);
                }
            }
            
            // Update progress every 512 bytes
            if ((addr & 0x1FF) == 0) {
                LED_RED_BLINK;  // Red for verifying (already correct)
            }
        }
        
        if (progress_callback) {
            char status[32];
            sprintf(status, "Verifying bank %lu/%lu", bank + 1, banks);
            progress_callback(status, bank + 1, banks);
        }
    }
    
    if (errors > 0) {
        char msg[64];
        sprintf(msg, "GB Flash: Verification failed with %lu errors\r\n", errors);
        soft_uart_send_string(msg);
        return false;
    }
    
    return true;
}

// Cart-specific requirement checks
bool gb_flash_check_cart_requirements(uint8_t mfg_id, uint8_t dev_id, uint16_t banks) {
    soft_uart_send_string("GB Flash: Checking cart requirements\r\n");
    
    // Check if this is a FunnyPlaying RTC cart with F3:C3 chip
    if (mfg_id == 0xF3 && dev_id == 0xC3) {
        // Try to read the cart header to check if it looks valid
        dataIn_GB();
        uint8_t header_check[4];
        for (int i = 0; i < 4; i++) {
            header_check[i] = readByte_GB(0x0100 + i);
        }
        
        // Check for Nintendo logo or valid header data
        // If we can't read proper data, the switch is likely in the wrong position
        if (header_check[0] == 0xFF && header_check[1] == 0xFF && 
            header_check[2] == 0xFF && header_check[3] == 0xFF) {
            // All FFs means we might be reading the flash chip directly
            // This could mean the switch is in flash position already
            soft_uart_send_string("GB Flash: F3:C3 Switch may already be in flash position\r\n");
        } else if (header_check[0] == 0xCE || header_check[0] == 0x00) {
            // Valid cart header - switch is in play position
            soft_uart_send_string("GB Flash: WARNING - FunnyPlaying RTC cart switch needs to be in flash position!\r\n");
            return false;  // Indicate check failed, caller should display warning
        }
    }
    
    // Check if this is a FunnyPlaying MidnightTrace cart (01:01)
    if (mfg_id == 0x01 && dev_id == 0x01) {
        soft_uart_send_string("GB Flash: MidnightTrace cart detected, checking MBC mode\r\n");
        
        // MidnightTrace carts need to be in MBC5 mode for flashing
        // The cart should support at least 512 banks (8MB) in MBC5 mode
        if (banks < 512) {
            soft_uart_send_string("GB Flash: WARNING - MidnightTrace cart must be in MBC5 mode!\r\n");
            char msg[128];
            sprintf(msg, "GB Flash: Cart only shows %d banks, need 512 for 8MB\r\n", banks);
            soft_uart_send_string(msg);
            return false;  // Indicate check failed
        } else {
            soft_uart_send_string("GB Flash: MidnightTrace cart appears to be in MBC5 mode\r\n");
            
            // Try to do a basic bank switch test
            dataOut_GB();
            writeByte_GB_WithPin(0x2000, 0x01, WRITE_PIN_WR);  // Try to set bank 1
            delay(10);
            writeByte_GB_WithPin(0x2000, 0x00, WRITE_PIN_WR);  // Back to bank 0
            dataIn_GB();
            
            soft_uart_send_string("GB Flash: Bank switching test complete\r\n");
        }
    }
    
    return true;  // All checks passed
}

// Chip erase
bool gb_flash_erase_chip(const flash_chip_info_t* chip) {
    if (!chip) {
        soft_uart_send_string("GB Flash: erase_chip - no chip info\r\n");
        return false;
    }
    
    soft_uart_send_string("GB Flash: Starting chip erase\r\n");
    char msg[128];
    sprintf(msg, "GB Flash: Chip = %s\r\n", chip->name);
    soft_uart_send_string(msg);
    
    dataOut_GB();
    
    // Exit any CFI mode first for CFI chips
    if (chip->type == FLASH_CFI && chip->needs_cfi_exit) {
        soft_uart_send_string("GB Flash: Exiting CFI mode before erase\r\n");
        writeByte_GB_WithPin(0x0000, CMD_RESET, chip->write_pin);
        writeByte_GB_WithPin(0x5555, CMD_RESET, chip->write_pin);
        delay(100);
    }
    
    // Execute chip erase sequence
    sprintf(msg, "GB Flash: chip->commands=%p\r\n", chip->commands);
    soft_uart_send_string(msg);
    if (chip->commands) {
        sprintf(msg, "GB Flash: chip->commands->chip_erase_count=%d\r\n", chip->commands->chip_erase_count);
        soft_uart_send_string(msg);
    }
    
    if (chip->commands && chip->commands->chip_erase_count > 0) {
        sprintf(msg, "GB Flash: Sending %d chip erase commands\r\n", chip->commands->chip_erase_count);
        soft_uart_send_string(msg);
        
        gb_flash_execute_commands_with_chip(chip->commands->chip_erase, 
                                           chip->commands->chip_erase_count,
                                           chip->write_pin, 0, chip);
    } else {
        // Fallback to standard sequence
        soft_uart_send_string("GB Flash: Using fallback chip erase sequence\r\n");
        writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_UNLOCK1, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_555, CMD_UNLOCK2, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_ERASE, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_UNLOCK1, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_555, CMD_UNLOCK2, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_CHIP_ERASE, chip->write_pin);
    }
    
    soft_uart_send_string("GB Flash: Chip erase command sent, waiting...\r\n");
    
    // Wait for erase to complete
    dataIn_GB();
    
    // Special handling for F3:C3 chips
    if (chip->manufacturer_id == 0xF3 && chip->device_id == 0xC3) {
        soft_uart_send_string("GB Flash: F3:C3 chip detected, using extended timeout\r\n");
        
        // Initial delay before polling
        delay(1000);
        
        // Poll using toggle bit method
        uint32_t timeout_ms = chip->chip_erase_timeout_ms > 0 ? chip->chip_erase_timeout_ms : 120000;
        uint32_t elapsed_ms = 1000;
        uint8_t last_read = readByte_GB(0);
        uint8_t current_read;
        
        while (elapsed_ms < timeout_ms) {
            current_read = readByte_GB(0);
            
            // Check if DQ6 stopped toggling (indicates completion)
            if ((last_read & 0x40) == (current_read & 0x40)) {
                // Double-check by reading again
                delay(10);
                uint8_t verify_read = readByte_GB(0);
                if ((current_read & 0x40) == (verify_read & 0x40)) {
                    soft_uart_send_string("GB Flash: F3:C3 chip erase complete\r\n");
                    return true;
                }
            }
            
            last_read = current_read;
            delay(100);
            elapsed_ms += 100;
            
            // Blink LED and update progress
            if ((elapsed_ms % 1000) == 0) {
                LED_RED_BLINK;
                
                // Call progress callback if available
                if (gb_progress_callback) {
                    int percentage = (elapsed_ms * 100) / timeout_ms;
                    if (percentage > 100) percentage = 100;
                    gb_progress_callback("Chip erase", percentage, 100);
                }
            }
        }
        
        soft_uart_send_string("GB Flash: F3:C3 chip erase timeout!\r\n");
        return false;
    }
    
    // Standard erase wait for other chips
    bool use_dq7 = (chip->type == FLASH_29F || chip->type == FLASH_CFI || chip->type == FLASH_29F160);
    uint32_t timeout_ms = chip->chip_erase_timeout_ms > 0 ? chip->chip_erase_timeout_ms : 60000;
    
    sprintf(msg, "GB Flash: Waiting up to %lums for erase\r\n", timeout_ms);
    soft_uart_send_string(msg);
    
    uint32_t elapsed_ms = 0;
    while (elapsed_ms < timeout_ms) {
        uint8_t status = readByte_GB(0);
        
        if (use_dq7) {
            // DQ7 method - bit 7 should read 1 when complete
            if (status & 0x80) {
                soft_uart_send_string("GB Flash: Chip erase complete (DQ7)\r\n");
                return true;
            }
        } else {
            // Direct comparison - should read 0xFF when complete
            if (status == 0xFF) {
                soft_uart_send_string("GB Flash: Chip erase complete\r\n");
                return true;
            }
        }
        
        delay(100);
        elapsed_ms += 100;
        
        // Blink LED periodically
        if ((elapsed_ms % 1000) == 0) {
            LED_RED_BLINK;  // Red for erasing
            
            // Call progress callback if available
            if (gb_progress_callback) {
                int percentage = (elapsed_ms * 100) / timeout_ms;
                if (percentage > 100) percentage = 100;
                gb_progress_callback("Chip erase", percentage, 100);
            }
        }
    }
    
    sprintf(msg, "GB Flash: Chip erase timeout after %lums\r\n", elapsed_ms);
    soft_uart_send_string(msg);
    return false;
}

// Erase sector
bool gb_flash_erase_sector(const flash_chip_info_t* chip, uint32_t address) {
    if (!chip) {
        return false;
    }
    
    dataOut_GB();
    
    // Don't support sector erase for certain types
    if (chip->type == FLASH_W29C020 || chip->type == FLASH_28LF040 || chip->type == FLASH_AT29LV) {
        return false;  // These chips don't support sector erase
    }
    
    // Execute erase sequence
    if (chip->commands && chip->commands->sector_erase_count > 0) {
        gb_flash_execute_commands_with_chip(chip->commands->sector_erase,
                                          chip->commands->sector_erase_count,
                                          chip->write_pin, address, chip);
    } else {
        // Fallback sequence
        writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_UNLOCK1, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_555, CMD_UNLOCK2, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_ERASE, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_UNLOCK1, chip->write_pin);
        writeByte_GB_WithPin(CMD_ADDR_555, CMD_UNLOCK2, chip->write_pin);
        writeByte_GB_WithPin(address, CMD_SECTOR_ERASE, chip->write_pin);
    }
    
    // Wait for erase to complete
    dataIn_GB();
    return gb_flash_wait_ready(chip->sector_erase_timeout_ms > 0 ? 
                              chip->sector_erase_timeout_ms : 3000);
}

// Check if flash is busy
bool gb_flash_check_busy(void) {
    dataIn_GB();
    uint8_t status = readByte_GB(0);
    
    // DQ7 = 0 means busy
    return !(status & 0x80);
}

// Wait for flash ready
bool gb_flash_wait_ready(uint32_t timeout_ms) {
    dataIn_GB();
    uint32_t elapsed = 0;
    
    while (elapsed < timeout_ms) {
        if (!gb_flash_check_busy()) {
            return true;
        }
        delay(1);
        elapsed++;
    }
    
    return false;
}

// New wait ready function that checks a specific address with toggle bit method
bool gb_flash_wait_ready_at(uint16_t address, uint32_t timeout_ms) {
    dataIn_GB();
    uint32_t loops = 0;
    uint32_t max_loops = timeout_ms * 100; // Check every 10us
    
    uint8_t last_read = readByte_GB(address);
    uint8_t current_read;
    
    while (loops < max_loops) {
        current_read = readByte_GB(address);
        
        // Check if DQ6 stopped toggling
        if ((last_read & 0x40) == (current_read & 0x40)) {
            // Double-check to make sure
            delayMicroseconds(10);
            uint8_t verify_read = readByte_GB(address);
            if ((current_read & 0x40) == (verify_read & 0x40)) {
                return true;  // Toggle bit stopped, operation complete
            }
        }
        
        // Check for error condition (DQ5 = 1 when DQ7 != expected)
        if (current_read & 0x20) {
            // Exceeded time limit error
            soft_uart_send_string("GB Flash: Wait timeout - DQ5 error\r\n");
            return false;
        }
        
        last_read = current_read;
        loops++;
        delayMicroseconds(10);
    }
    
    char msg[64];
    sprintf(msg, "GB Flash: Wait timeout at address 0x%04X after %lums\r\n", address, timeout_ms);
    soft_uart_send_string(msg);
    return false;
}

// Wait for sector erase to complete - fixed version
bool gb_flash_wait_sector_erase(const flash_chip_info_t* chip, uint16_t sector_addr, uint32_t timeout_ms) {
    // For erase operations, we should check the sector address
    return gb_flash_wait_ready_at(sector_addr, timeout_ms);
}

// New write byte function using chip database
bool gb_flash_write_byte_new(const flash_chip_info_t* chip, uint16_t bank, uint16_t address, uint8_t data) {
    if (!chip) {
        return false;
    }
    
    // Debug writes - show first 10 and then every 1000th write
    static int write_count = 0;
    if (write_count < 10 || (write_count % 1000) == 0 || address == 0x4000) {
        char msg[128];
        sprintf(msg, "GB Flash: Write #%d - bank %d, addr 0x%04X, data 0x%02X\r\n", 
                write_count, bank, address, data);
        soft_uart_send_string(msg);
    }
    write_count++;
    
    // Bank switching optimization: only switch if we're not already in the correct bank
    static uint16_t current_bank = 0xFFFF; // Invalid bank to force first switch
    
    dataOut_GB();
    
    if (current_bank != bank) {
        // Debug bank switches
        if (write_count < 100 || (write_count % 1000) == 0) {
            char bank_msg[64];
            sprintf(bank_msg, "GB Flash: Switching to bank %d\r\n", bank);
            soft_uart_send_string(bank_msg);
        }
        gb_flash_switch_bank(bank);
        delayMicroseconds(100);
        current_bank = bank;
    }
    
    // Special handling for F3:C3 CFI clone chips
    bool is_f3c3 = (chip->manufacturer_id == 0xF3 && chip->device_id == 0xC3);
    
    // Debug special chip handling
    if (write_count == 0) {
        if (is_f3c3) {
            soft_uart_send_string("GB Flash: Using F3:C3 special handling (compensated AUDIO pin)\r\n");
        } else if (chip->manufacturer_id == 0x01 && chip->device_id == 0x01) {
            soft_uart_send_string("GB Flash: MidnightTrace cart - standard AMD commands\r\n");
        }
    }
    
    if (is_f3c3) {
        // F3:C3 chips need special handling:
        // 1. GBxCart RW workaround from FlashGBX
        writeByte_GB_WithPin(0x0000, 0xFF, chip->write_pin);
        delayMicroseconds(1000);  // 1ms delay
        
        // 2. Exit CFI mode with multiple reset commands
        writeByte_GB_WithPin(0x0000, 0xF0, chip->write_pin);
        delayMicroseconds(100);
        writeByte_GB_WithPin(0x5555, 0xF0, chip->write_pin);
        delayMicroseconds(100);
        writeByte_GB_WithPin(0xAAAA, 0xF0, chip->write_pin);
        delayMicroseconds(100);
        
        // 3. For F3:C3, use the special unlock codes (0xA9/0x56) with compensation
        // Based on FlashGBX implementation + midnight trace procedure
        gb_flash_write_byte_compensated(0xAAA, 0xA9, false, false, chip->write_pin);
        delayMicroseconds(10);
        gb_flash_write_byte_compensated(0x555, 0x56, false, false, chip->write_pin);
        delayMicroseconds(10);
        gb_flash_write_byte_compensated(0xAAA, 0xA0, false, false, chip->write_pin);
        delayMicroseconds(10);
    } else {
        // Original logic for other chips
        // For CFI chips, always ensure we're out of CFI mode before writing
        if (chip->type == FLASH_CFI) {
            writeByte_GB_WithPin(0x0000, 0xF0, chip->write_pin);
            delayMicroseconds(100);
        }
        
        // For some CFI chips in higher banks, write commands may need to go to bank 0
        // But MidnightTrace (01:01) doesn't need this
        if (chip->type == FLASH_CFI && bank > 0 && 
            !(chip->manufacturer_id == 0x01 && chip->device_id == 0x01)) {
            // Temporarily switch to bank 0 for command sequence
            gb_flash_switch_bank(0);
            delayMicroseconds(500); // Increased delay for CFI stability
            current_bank = 0; // Update tracking
        }
        
        // Execute write sequence from chip database
        if (chip->commands && chip->commands->write_sequence_count > 0) {
            gb_flash_execute_commands_with_chip(chip->commands->write_sequence,
                                              chip->commands->write_sequence_count,
                                              chip->write_pin, 0, chip);
        } else {
            // Fallback sequence
            writeByte_GB_WithPin(0xAAA, 0xAA, chip->write_pin);
            writeByte_GB_WithPin(0x555, 0x55, chip->write_pin);
            writeByte_GB_WithPin(0xAAA, 0xA0, chip->write_pin);
        }
        
        // Switch back to target bank for CFI chips that needed bank 0
        if (chip->type == FLASH_CFI && bank > 0 && 
            !(chip->manufacturer_id == 0x01 && chip->device_id == 0x01)) {
            gb_flash_switch_bank(bank);
            delayMicroseconds(500); // Increased delay for CFI stability
            current_bank = bank; // Update tracking
        }
    }
    
    // Write the data
    // MidnightTrace write procedure: Use compensated write with AUDIO pin for F3:C3
    if (is_f3c3) {
        // F3:C3 chips use AUDIO pin for data writes with compensation
        // This is the "midnight trace write procedure"
        gb_flash_write_byte_compensated(address, data, false, false, WRITE_PIN_AUDIO);
    } else {
        // Standard chips use their designated write pin
        writeByte_GB_WithPin(address, data, chip->write_pin);
    }
    
    // Handle polling based on write pin type
    if (chip->write_pin == WRITE_PIN_AUDIO) {
        // Fixed delay for AUDIO pin
        uint32_t delay_us = chip->write_timeout_us ? chip->write_timeout_us : 250;
        delayMicroseconds(delay_us);
    } else {
        // Status polling for WR pin
        dataIn_GB();
        
        // Special handling for F3:C3 chips
        if (is_f3c3) {
            // F3:C3 chips need longer delays and different polling
            delayMicroseconds(500);  // Initial delay
            
            // Try data polling with DQ7/DQ6 method
            uint8_t status1 = readByte_GB(address);
            uint8_t status2;
            int retry_count = 0;
            const int max_retries = 100;
            
            while (retry_count < max_retries) {
                delayMicroseconds(50);
                status2 = readByte_GB(address);
                
                // Check if DQ6 toggling stopped or data matches
                if ((status1 & 0x40) == (status2 & 0x40) || status2 == data) {
                    // One more read to confirm
                    delayMicroseconds(50);
                    uint8_t verify = readByte_GB(address);
                    if (verify == data) {
                        dataOut_GB();
                        return true;
                    }
                }
                
                // Check for error (DQ5 = 1 when DQ7 != data bit 7)
                if ((status2 & 0x20) && ((status2 & 0x80) != (data & 0x80))) {
                    char msg[128];
                    sprintf(msg, "GB Flash: F3:C3 write timeout at bank %d, addr 0x%04X\r\n", bank, address);
                    soft_uart_send_string(msg);
                    dataOut_GB();
                    return false;
                }
                
                status1 = status2;
                retry_count++;
            }
            
            char msg[128];
            sprintf(msg, "GB Flash: F3:C3 write max retries at bank %d, addr 0x%04X\r\n", bank, address);
            soft_uart_send_string(msg);
            dataOut_GB();
            return false;
        } else {
            // Standard polling for other chips
            uint32_t timeout_us = chip->write_timeout_us ? chip->write_timeout_us : 100;
            
            // For CFI chips, use toggle bit method with longer timeout
            if (chip->type == FLASH_CFI) {
                uint32_t elapsed = 0;
                uint8_t last_read = readByte_GB(address);
                // CFI chips need much longer timeout - use 10ms 
                uint32_t cfi_timeout_us = 10000;
                
                while (elapsed < cfi_timeout_us) {
                    uint8_t current_read = readByte_GB(address);
                    
                    // Check if data matches (primary check) or DQ6 stopped toggling
                    if (current_read == data) {
                        // Verify with one more read
                        delayMicroseconds(10);
                        if (readByte_GB(address) == data) {
                            dataOut_GB();
                            return true;
                        }
                    }
                    
                    // Check DQ5 timeout bit
                    if (current_read & 0x20) {
                        // DQ5 = 1 indicates timeout/error
                        // Re-read to confirm
                        uint8_t reread = readByte_GB(address);
                        if (reread == data) {
                            // False alarm, write succeeded
                            dataOut_GB();
                            return true;
                        } else {
                            // Real timeout
                            // Always show DQ5 timeout as it's a chip-reported error
                            char err_msg[64];
                            sprintf(err_msg, "GB Flash: DQ5 timeout at addr 0x%04X\r\n", address);
                            soft_uart_send_string(err_msg);
                            // Reset the chip after timeout
                            dataOut_GB();
                            writeByte_GB_WithPin(0x0000, 0xF0, chip->write_pin);
                            delayMicroseconds(100);
                            return false;
                        }
                    }
                    
                    // Also check toggle bit
                    if ((last_read & 0x40) == (current_read & 0x40)) {
                        // Verify toggle stopped
                        delayMicroseconds(10);
                        uint8_t verify = readByte_GB(address);
                        if ((current_read & 0x40) == (verify & 0x40)) {
                            // Toggle stopped, check if data matches
                            if (verify == data) {
                                dataOut_GB();
                                return true;
                            }
                        }
                    }
                    
                    last_read = current_read;
                    elapsed += 10;
                    delayMicroseconds(10);
                }
                
                // Timeout - debug message (only show first few and periodic)
                static int timeout_count = 0;
                timeout_count++;
                if (timeout_count <= 10 || (timeout_count % 1000) == 0) {
                    char timeout_msg[128];
                    sprintf(timeout_msg, "GB Flash: CFI write timeout #%d at bank %d addr 0x%04X (elapsed %luus)\r\n", 
                            timeout_count, bank, address, elapsed);
                    soft_uart_send_string(timeout_msg);
                }
            } else {
                // DQ7 polling for standard chips
                if (!gb_flash_poll_status(address, data, timeout_us)) {
                    dataOut_GB();
                    return false;
                }
            }
        }
    }
    
    dataOut_GB();
    return true;
}

// Write single byte
bool gb_flash_write_byte(const flash_chip_info_t* chip, uint32_t address, uint8_t data) {
    if (!chip) {
        return false;
    }
    
    dataOut_GB();
    
    // Different algorithms for different chip types
    switch (chip->type) {
        case FLASH_W29C020:
        case FLASH_28LF040:
            // Direct write for EEPROM types
            writeByte_GB_WithPin(address, data, chip->write_pin);
            // Long write cycle for EEPROM
            delay(10);
            break;
            
        case FLASH_AT29LV:
            // Atmel uses page programming - should use gb_flash_write_page instead
            soft_uart_send_string("GB Flash: AT29LV requires page programming\r\n");
            return false;
            
        default:
            // Standard flash programming sequence
            if (chip->commands && chip->commands->write_sequence_count > 0) {
                gb_flash_execute_commands_with_chip(chip->commands->write_sequence,
                                                  chip->commands->write_sequence_count,
                                                  chip->write_pin, 0, chip);
            } else {
                // Fallback sequence
                writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_UNLOCK1, chip->write_pin);
                writeByte_GB_WithPin(CMD_ADDR_555, CMD_UNLOCK2, chip->write_pin);
                writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_PROGRAM, chip->write_pin);
            }
            
            // Write data
            writeByte_GB_WithPin(address, data, chip->write_pin);
            
            // Wait for completion
            dataIn_GB();
            
            // Use appropriate timeout
            uint32_t timeout_us = chip->write_timeout_us > 0 ? chip->write_timeout_us : 100;
            
            if (!gb_flash_poll_status(address, data, timeout_us)) {
                return false;
            }
            break;
    }
    
    return true;
}

// Write buffer
bool gb_flash_write_buffer(const flash_chip_info_t* chip, uint32_t address, uint8_t* buffer, uint32_t length) {
    if (!chip || !buffer) {
        return false;
    }
    
    for (uint32_t i = 0; i < length; i++) {
        if (!gb_flash_write_byte(chip, address + i, buffer[i])) {
            return false;
        }
        
        // Progress indicator
        if ((i & 0xFF) == 0) {
            LED_BLUE_BLINK;  // Blue for writing
        }
    }
    
    return true;
}

// Switch ROM bank
void gb_flash_switch_bank(uint8_t bank) {
    dataOut_GB();
    writeByte_GB_WithPin(0x2100, bank & 0xFF, WRITE_PIN_WR);
    if (bank > 255) {
        // For banks 256-511, set bit 9 at 0x3000
        writeByte_GB_WithPin(0x3000, (bank >> 8) & 0x01, WRITE_PIN_WR);
    }
    // Small delay for bank switch to take effect
    delayMicroseconds(10);
}

// Read CFI query
bool gb_flash_read_cfi(void) {
    soft_uart_send_string("GB Flash: Reading CFI\r\n");
    
    dataOut_GB();
    
    // Enter CFI Query mode
    writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_RESET, WRITE_PIN_WR);
    delay(10);
    writeByte_GB_WithPin(CMD_ADDR_AAA, CMD_CFI_QUERY, WRITE_PIN_WR);
    delay(10);
    
    dataIn_GB();
    
    // Check for "QRY" signature at 0x10-0x12
    uint8_t q = readByte_GB(0x10);
    uint8_t r = readByte_GB(0x11);
    uint8_t y = readByte_GB(0x12);
    
    char msg[64];
    sprintf(msg, "GB Flash: CFI signature: %c%c%c\r\n", q, r, y);
    soft_uart_send_string(msg);
    
    if (q == 'Q' && r == 'R' && y == 'Y') {
        soft_uart_send_string("GB Flash: Valid CFI signature found\r\n");
        
        // Read device size
        uint32_t device_size = 1 << readByte_GB(0x27);
        sprintf(msg, "GB Flash: CFI device size: %lu bytes\r\n", device_size);
        soft_uart_send_string(msg);
        
        // Read region count
        uint8_t region_count = readByte_GB(0x2C);
        sprintf(msg, "GB Flash: CFI region count: %d\r\n", region_count);
        soft_uart_send_string(msg);
        
        // Exit CFI mode
        dataOut_GB();
        writeByte_GB_WithPin(0, CMD_RESET, WRITE_PIN_WR);
        delay(10);
        
        return true;
    }
    
    // Exit CFI mode even if signature not found
    dataOut_GB();
    writeByte_GB_WithPin(0, CMD_RESET, WRITE_PIN_WR);
    delay(10);
    
    return false;
}

// Write page (for Atmel chips)
bool gb_flash_write_page(const flash_chip_info_t* chip, uint32_t address, uint8_t* data, uint16_t length) {
    if (!chip || !data) {
        return false;
    }
    
    if (chip->type != FLASH_AT29LV) {
        // Use byte programming for non-Atmel chips
        return gb_flash_write_buffer(chip, address, data, length);
    }
    
    // Atmel page programming
    dataOut_GB();
    
    // Software data protection unlock sequence
    writeByte_GB_WithPin(0x5555, 0xAA, chip->write_pin);
    writeByte_GB_WithPin(0x2AAA, 0x55, chip->write_pin);
    writeByte_GB_WithPin(0x5555, 0xA0, chip->write_pin);
    
    // Write page data
    for (uint16_t i = 0; i < length && i < chip->page_size; i++) {
        writeByte_GB_WithPin(address + i, data[i], chip->write_pin);
    }
    
    // Page write cycle time
    delay(20);
    
    return true;
}

// GB flash init
void gb_flash_init(void) {
    // Initialize GB flash subsystem if needed
}

// GBA flash functions (stubs for now)
void gba_flash_init(void) {
    // Initialize GBA flash subsystem if needed
}

bool gba_flash_detect(uint8_t* mfg_id, uint8_t* dev_id) {
    // GBA uses different addressing
    dataOut_GB();
    
    // Send ID command sequence to GBA cart
    writeWord_GBA(0x0AAA, 0x00AA);
    writeWord_GBA(0x0554, 0x0055);
    writeWord_GBA(0x0AAA, 0x0090);
    
    delay(10);
    
    // Read IDs
    *mfg_id = readByte_GBA(0);
    *dev_id = readByte_GBA(2);
    
    // Reset
    writeWord_GBA(0, 0x00F0);
    
    return (*mfg_id != 0xFF && *dev_id != 0xFF);
}

bool gba_flash_erase_chip(const flash_chip_info_t* chip) {
    if (!chip) return false;
    
    // GBA chip erase implementation
    // TODO: Implement based on chip type
    
    return false;
}

bool gba_flash_erase_sector(const flash_chip_info_t* chip, uint32_t address) {
    if (!chip) return false;
    
    // GBA sector erase implementation
    // TODO: Implement based on chip type
    
    return false;
}

bool gba_flash_write_byte(const flash_chip_info_t* chip, uint32_t address, uint8_t data) {
    if (!chip) return false;
    
    // GBA byte write implementation
    // TODO: Implement based on chip type
    
    return false;
}

void gba_flash_reset(void) {
    writeWord_GBA(0, 0x00F0);
}

bool gba_flash_write_buffer(const flash_chip_info_t* chip, uint32_t address, uint8_t* buffer, uint32_t length) {
    if (!chip || !buffer) {
        return false;
    }
    
    for (uint32_t i = 0; i < length; i++) {
        if (!gba_flash_write_byte(chip, address + i, buffer[i])) {
            return false;
        }
    }
    
    return true;
}

bool gba_flash_verify(const flash_chip_info_t* chip, uint32_t address, uint8_t* buffer, uint32_t length) {
    if (!chip || !buffer) {
        return false;
    }
    
    for (uint32_t i = 0; i < length; i++) {
        uint8_t read_val = readByte_GBA(address + i);
        if (read_val != buffer[i]) {
            return false;
        }
    }
    
    return true;
}


// Execute flash commands helper - with chip info for F3:C3 detection
void gb_flash_execute_commands_with_chip(const flash_command_t* commands, uint8_t count, 
                                        enum write_pin pin, uint32_t sector_addr,
                                        const flash_chip_info_t* chip) {
    // Check if we're dealing with F3:C3 chip
    bool use_compensated = false;
    if (chip && chip->manufacturer_id == 0xF3 && chip->device_id == 0xC3) {
        use_compensated = true;
    }
    
    for (uint8_t i = 0; i < count; i++) {
        uint16_t addr = commands[i].address;
        uint8_t data = commands[i].data;
        
        // Handle sector address placeholder
        if (addr == 0x0000 && data == 0x30) {
            addr = sector_addr;
        }
        
        // Use compensated writes for F3:C3 (midnight trace procedure)
        if (use_compensated) {
            gb_flash_write_byte_compensated(addr, data, false, false, pin);
        } else {
            writeByte_GB_WithPin(addr, data, pin);
        }
    }
}

// Legacy wrapper for compatibility
void gb_flash_execute_commands(const flash_command_t* commands, uint8_t count, enum write_pin pin, uint32_t sector_addr) {
    gb_flash_execute_commands_with_chip(commands, count, pin, sector_addr, NULL);
}


// New erase sector function using chip database
bool gb_flash_erase_sector_new(const flash_chip_info_t* chip, uint16_t bank, uint16_t sector_addr) {
    if (!chip) {
        soft_uart_send_string("GB Flash: erase_sector_new - no chip info\r\n");
        return false;
    }
    
    // Switch to correct bank
    dataOut_GB();
    gb_flash_switch_bank(bank);
    delayMicroseconds(100);
    
    // Execute erase sequence from chip database
    if (chip->commands && chip->commands->sector_erase_count > 0) {
        gb_flash_execute_commands_with_chip(chip->commands->sector_erase,
                                          chip->commands->sector_erase_count,
                                          chip->write_pin,
                                          sector_addr, chip);
    } else {
        // Fallback to standard erase sequence
        writeByte_GB_WithPin(0xAAA, 0xAA, chip->write_pin);
        writeByte_GB_WithPin(0x555, 0x55, chip->write_pin);
        writeByte_GB_WithPin(0xAAA, 0x80, chip->write_pin);
        writeByte_GB_WithPin(0xAAA, 0xAA, chip->write_pin);
        writeByte_GB_WithPin(0x555, 0x55, chip->write_pin);
        writeByte_GB_WithPin(sector_addr, 0x30, chip->write_pin);
    }
    
    // Wait for erase completion
    uint32_t timeout_ms = chip->sector_erase_timeout_ms ? 
                         chip->sector_erase_timeout_ms : 3000;
    
    return gb_flash_wait_sector_erase(chip, sector_addr, timeout_ms);
}