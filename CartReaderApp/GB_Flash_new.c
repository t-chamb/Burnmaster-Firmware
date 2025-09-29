// Execute a sequence of flash commands
void gb_flash_execute_commands(const flash_command_t* commands, uint8_t count, enum write_pin pin, uint32_t sector_addr) {
    for (uint8_t i = 0; i < count; i++) {
        uint16_t addr = commands[i].address;
        uint8_t data = commands[i].data;
        
        // Replace SA placeholder with actual sector address
        if (addr == 0x0000 && data == 0x30) {
            addr = sector_addr;
        }
        
        // Write the command
        writeByte_GB_WithPin(addr, data, pin);
    }
}

// Execute chip erase using chip-specific commands
bool gb_flash_erase_chip_new(const flash_chip_info_t* chip) {
    if (!chip || !chip->commands) {
        soft_uart_send_string("GB Flash: ERROR - chip or commands not specified\r\n");
        return false;
    }
    
    soft_uart_send_string("GB Flash: Starting chip erase with database commands\r\n");
    
    // Set data pins to output
    dataOut_GB();
    
    // Execute chip erase command sequence
    gb_flash_execute_commands(chip->commands->chip_erase, 
                             chip->commands->chip_erase_count,
                             chip->write_pin,
                             0);
    
    // Wait for erase to complete
    dataIn_GB();
    
    uint32_t timeout_ms = chip->chip_erase_timeout_ms;
    uint32_t elapsed_ms = 0;
    const uint32_t poll_interval_ms = 100;
    
    while (elapsed_ms < timeout_ms) {
        uint8_t status = readByte_GB(0);
        if (status == 0xFF) {
            soft_uart_send_string("GB Flash: Chip erase complete\r\n");
            return true;
        }
        
        delay(poll_interval_ms);
        elapsed_ms += poll_interval_ms;
        
        // Progress update
        if ((elapsed_ms % 1000) == 0) {
            char msg[64];
            sprintf(msg, "GB Flash: Erasing... %lu/%lu seconds\r\n", 
                    elapsed_ms / 1000, timeout_ms / 1000);
            soft_uart_send_string(msg);
        }
    }
    
    soft_uart_send_string("GB Flash: Chip erase timeout!\r\n");
    return false;
}