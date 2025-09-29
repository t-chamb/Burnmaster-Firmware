# Burnmaster Firmware Refactoring Summary

## Overview
This document summarizes the major refactoring work done on the Burnmaster firmware to make it fully universal and database-driven, with no hardcoded chip-specific logic.

## Version History
- **1.14**: Original version with hardcoded flash chip support
- **1.15**: Complete refactoring for universal flash support with database-driven architecture

## Key Changes

### 1. Universal Flash Database (`GB_Flash.c`)
- Moved all flash chip definitions to a centralized database
- Each chip entry contains all necessary parameters:
  - Manufacturer and device IDs
  - Size and sector information
  - Flash type (CFI, AMD, Intel, etc.)
  - Write pin configuration (WR vs AUDIO)
  - Timing parameters
  - Command set pointers
- Added support for special chips:
  - F3:C3 chips with modified unlock codes (0xA9/0x56)
  - MidnightTrace 8MB cart (01:01)

### 2. Dynamic Flash Operations
- Implemented universal flash operation functions:
  - `gb_flash_write_chip()`: Universal write with progress callback
  - `gb_flash_erase_chip()`: Universal erase with proper CFI mode handling
  - `gb_flash_wait_ready_at()`: Toggle bit polling at specific addresses
- All operations now use chip database parameters instead of hardcoded values
- Progress callbacks provide real-time updates to UI

### 3. Cart-Specific Requirements
- Added `gb_flash_check_cart_requirements()` function
- Detects special cart requirements:
  - FunnyPlaying RTC carts with physical switch
  - MidnightTrace carts requiring MBC5 mode
  - Other cart-specific initialization needs

### 4. UI Improvements
- Fixed progress display calculation using 64-bit math
- Added color-coded LED indicators:
  - Red: Erasing
  - Blue: Writing/Normal operation
  - Red: Verifying
- Improved error messages and user feedback
- Fixed progress bar overflow issues

### 5. Bug Fixes
- Fixed buffer indexing bug in write operations
- Fixed DQ5 timeout bit checking
- Disabled bank 0 switching for MidnightTrace chips
- Fixed CFI mode exit before erase operations
- Increased timeouts for CFI chips
- Fixed auto-detect completion handling

### 6. Code Organization
- All flash-specific code moved to `GB_Flash.c`
- Main `GB.c` now universally executable for any flash ID and cart
- No more hardcoded chip-specific logic in main files
- Clean separation of concerns

## Testing Results
Successfully tested with:
- Standard flash chips (AMD, SST, etc.)
- F3:C3 chips with special unlock codes
- MidnightTrace 8MB carts
- Various other flash carts

## Future Improvements
- Add more chip entries to database as discovered
- Implement additional progress indicators
- Add support for more exotic flash types
- Improve error recovery mechanisms

## Repository Cleanup
- Removed all external firmware versions from tracking
- Removed FlashGBX and Sanni cart reader files
- Added comprehensive .gitignore files
- Now only tracking active development in Burnmaster-Firmware-1.12

## Important Notes
- **USB Device Issue**: Some flash operations may fail if `/dev/cu.usbmodemF103_CART_V121` is not present. This appears to be a macOS-specific enumeration issue.
- **Cart Switch Position**: FunnyPlaying RTC carts must have their physical switch in the correct position for flashing to work.
- **MBC Mode**: Some carts (like MidnightTrace) require specific MBC modes to be set before operations.