#ifndef GB_FLASH_H
#define GB_FLASH_H

#include <stdint.h>

// Flash chip manufacturer IDs
#define MFG_AMD         0x01
#define MFG_FUJITSU     0x04
#define MFG_SST         0xBF
#define MFG_SANYO       0x62
#define MFG_INTEL       0x89
#define MFG_SHARP       0xB0
#define MFG_ATMEL       0x1F
#define MFG_MACRONIX    0xC2

// GB Flash chip device IDs
#define DEV_29F016B     0xAD
#define DEV_29F016D     0xAD
#define DEV_29F032B     0x41
#define DEV_29F033C     0xD4
#define DEV_29F080B     0xD5
#define DEV_29F160      0xD2
#define DEV_29F160_ALT  0xD8
#define DEV_29EE020     0x45
#define DEV_W29C020     0x10
#define DEV_SST28LF040  0x04
#define DEV_SST39SF010  0xB5
#define DEV_SST39SF020  0xB6
#define DEV_SST39SF040  0xB7

// GBA Flash chip device IDs
#define DEV_AT29LV512   0x3D
#define DEV_SST39VF512  0xD4
#define DEV_MX29L512    0x1C
#define DEV_MN63F805MNP 0x1B
#define DEV_MX29L010    0x09
#define DEV_LE26FV10N1TS 0x13
#define DEV_4000L0YBQ0  0x02
#define DEV_4400L0ZDQ0  0x16
#define DEV_F0088H0     0x12
#define DEV_MX29GL128E  0x7E

// Command addresses for different algorithms
#define CMD_ADDR_555    0x555
#define CMD_ADDR_2AA    0x2AA
#define CMD_ADDR_5555   0x5555
#define CMD_ADDR_2AAA   0x2AAA
#define CMD_ADDR_AAA    0xAAA
#define CMD_ADDR_3555   0x3555

// Flash commands
#define CMD_UNLOCK1     0xAA
#define CMD_UNLOCK2     0x55
#define CMD_AUTOSELECT  0x90
#define CMD_PROGRAM     0xA0
#define CMD_ERASE       0x80
#define CMD_CHIP_ERASE  0x10
#define CMD_SECTOR_ERASE 0x30
#define CMD_RESET       0xF0
#define CMD_CFI_QUERY   0x98
#define CMD_BANK_SWITCH 0xB0

// MX29GL256EL specific commands
#define CMD_MX_UNLOCK1  0xA9
#define CMD_MX_UNLOCK2  0x56

// Flash chip types
enum flash_type {
    FLASH_UNKNOWN = 0,
    FLASH_29F,      // Standard 29F algorithm
    FLASH_39SF,     // SST 39SF algorithm
    FLASH_29F160,   // 29F160 algorithm
    FLASH_W29C020,  // W29C020/29EE020 algorithm
    FLASH_28LF040,  // SST 28LF040 algorithm
    FLASH_AT29LV,   // Atmel page program algorithm
    FLASH_CFI,      // CFI compliant chip
    FLASH_MX29GL    // MX29GL256EL style chips
};

// Write pin options
enum write_pin {
    WRITE_PIN_WR = 0,     // Use WR pin (standard)
    WRITE_PIN_AUDIO = 1   // Use AUDIO pin (some DIY carts)
};

// Command sequence structure
typedef struct {
    uint16_t address;
    uint8_t data;
} flash_command_t;

// Command set structure - similar to FlashGBX's approach
typedef struct {
    // Unlock sequences
    uint16_t unlock1_addr;
    uint8_t unlock1_data;
    uint16_t unlock2_addr;
    uint8_t unlock2_data;
    
    // ID entry sequence (up to 3 commands)
    flash_command_t id_entry[3];
    uint8_t id_entry_count;
    
    // Erase sequences
    flash_command_t chip_erase[6];
    uint8_t chip_erase_count;
    flash_command_t sector_erase[6];
    uint8_t sector_erase_count;
    
    // Write sequence
    flash_command_t write_sequence[4];
    uint8_t write_sequence_count;
    
    // Reset command
    uint16_t reset_addr;
    uint8_t reset_data;
} flash_commands_t;

// Flash chip information structure
typedef struct {
    uint8_t manufacturer_id;
    uint8_t device_id;
    const char* name;
    uint32_t size;
    uint32_t sector_size;
    uint16_t page_size;     // For page programming chips
    enum flash_type type;
    bool needs_bank_switch;
    uint8_t voltage;        // 3 or 5 for 3.3V or 5V
    enum write_pin write_pin;  // Which pin to use for write operations
    
    // Timing parameters
    uint16_t chip_erase_timeout_ms;    // Chip erase timeout in milliseconds
    uint16_t sector_erase_timeout_ms;  // Sector erase timeout in milliseconds
    uint16_t write_timeout_us;         // Byte program timeout in microseconds
    
    // Command set
    const flash_commands_t* commands;  // Pointer to command sequences
    
    // Chip-specific quirks
    bool needs_cfi_exit;  // Some chips need explicit CFI mode exit before operations
} flash_chip_info_t;

// Function prototypes for GB flash operations
void gb_flash_init(void);
bool gb_flash_detect(uint8_t* mfg_id, uint8_t* dev_id);
void gb_flash_execute_commands(const flash_command_t* commands, uint8_t count, enum write_pin pin, uint32_t sector_addr);
bool gb_flash_detect_with_pin(uint8_t* mfg_id, uint8_t* dev_id, enum write_pin pin);
bool gb_flash_read_cfi(void);
const flash_chip_info_t* gb_flash_get_chip_info(uint8_t mfg_id, uint8_t dev_id);
const flash_chip_info_t* gb_flash_detect_and_match(enum write_pin pin);
bool gb_flash_erase_chip(const flash_chip_info_t* chip);
bool gb_flash_erase_sector(const flash_chip_info_t* chip, uint32_t address);
bool gb_flash_write_byte(const flash_chip_info_t* chip, uint32_t address, uint8_t data);
bool gb_flash_write_page(const flash_chip_info_t* chip, uint32_t address, uint8_t* data, uint16_t length);
bool gb_flash_write_buffer(const flash_chip_info_t* chip, uint32_t address, uint8_t* buffer, uint32_t length);
void gb_flash_reset(void);
void gb_flash_reset_with_pin(enum write_pin pin);
bool gb_flash_check_busy(void);
bool gb_flash_wait_ready(uint32_t timeout_ms);
void gb_flash_switch_bank(uint8_t bank);

// New composable functions using chip database
bool gb_flash_erase_sector_new(const flash_chip_info_t* chip, uint16_t bank, uint16_t sector_addr);
bool gb_flash_write_byte_new(const flash_chip_info_t* chip, uint16_t bank, uint16_t address, uint8_t data);

// Generic flash interface functions - these should be used from GB.c
bool gb_flash_identify_chip(uint8_t* mfg_id, uint8_t* dev_id, enum flash_type* type, 
                           bool* x16_mode, bool* switch_bits, uint16_t* banks);
bool gb_flash_enter_cfi_mode(bool x16_mode, enum write_pin pin);
void gb_flash_exit_cfi_mode(const flash_chip_info_t* chip);
bool gb_flash_poll_status(uint16_t address, uint8_t expected_data, uint32_t timeout_us);
bool gb_flash_wait_toggle_bit(uint16_t address, uint32_t timeout_us);
bool gb_flash_check_dq7(uint16_t address, uint8_t expected_data, uint32_t timeout_us);
bool gb_flash_wait_ready_at(uint16_t address, uint32_t timeout_ms);
bool gb_flash_erase_chip(const flash_chip_info_t* chip);
void gb_flash_write_byte_compensated(uint16_t address, uint8_t data, bool x16_mode, bool switch_bits, enum write_pin pin);
uint8_t gb_flash_read_byte_compensated(uint16_t address, bool x16_mode, bool switch_bits);
bool gb_flash_erase_with_progress(const flash_chip_info_t* chip, uint8_t rom_banks, 
                                 void (*progress_callback)(const char*, int, int));
bool gb_flash_write_with_progress(const flash_chip_info_t* chip, uint8_t* buffer, uint32_t size,
                                 void (*progress_callback)(const char*, int, int));
bool gb_flash_verify_with_progress(uint8_t* buffer, uint32_t size,
                                  void (*progress_callback)(const char*, int, int));

// Cart-specific checks (switch position, MBC mode, etc)
bool gb_flash_check_cart_requirements(uint8_t mfg_id, uint8_t dev_id, uint16_t banks);

// Function prototypes for GBA flash operations
void gba_flash_init(void);
bool gba_flash_detect(uint8_t* mfg_id, uint8_t* dev_id);
const flash_chip_info_t* gba_flash_get_chip_info(uint8_t mfg_id, uint8_t dev_id);
bool gba_flash_erase_chip(const flash_chip_info_t* chip);
bool gba_flash_erase_sector(const flash_chip_info_t* chip, uint32_t address);
bool gba_flash_write_byte(const flash_chip_info_t* chip, uint32_t address, uint8_t data);
bool gba_flash_write_page(const flash_chip_info_t* chip, uint32_t address, uint8_t* data, uint16_t length);
bool gba_flash_write_buffer(const flash_chip_info_t* chip, uint32_t address, uint8_t* buffer, uint32_t length);
void gba_flash_reset(void);
bool gba_flash_verify(const flash_chip_info_t* chip, uint32_t address, uint8_t* buffer, uint32_t length);

#endif // GB_FLASH_H