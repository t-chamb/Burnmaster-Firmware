#!/bin/bash

# Build script for Burnmaster Firmware
# Uses SEGGER Embedded Studio v6.22a

# WARNING: When monitoring USB serial output:
# DO NOT USE /dev/tty.usbserial-AB8IJVY9 - This is Joey Jr used by another session!
# The Burnmaster device uses a different serial number. Check with: ls /dev/tty.usbserial*

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building Burnmaster Firmware...${NC}"

# Change to CartReaderApp directory
cd CartReaderApp

# Check if emBuild is in PATH, otherwise use full path
if command -v emBuild &> /dev/null; then
    EMBUILD="emBuild"
else
    # Try common installation paths
    if [ -f "/Applications/SEGGER Embedded Studio for ARM 6.22a/bin/emBuild" ]; then
        EMBUILD="/Applications/SEGGER Embedded Studio for ARM 6.22a/bin/emBuild"
    elif [ -f "/opt/SEGGER/SEGGER Embedded Studio for ARM 6.22a/bin/emBuild" ]; then
        EMBUILD="/opt/SEGGER/SEGGER Embedded Studio for ARM 6.22a/bin/emBuild"
    else
        echo -e "${RED}Error: emBuild not found! Please install SEGGER Embedded Studio v6.22a${NC}"
        exit 1
    fi
fi

# Build the project
echo -e "${YELLOW}Running: $EMBUILD -config Debug GDCartReader.emProject${NC}"
"$EMBUILD" -config Debug GDCartReader.emProject

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful!${NC}"
    
    # Copy GDCartReader.bin to update.bin
    cp Output/Debug/Exe/GDCartReader.bin Output/Debug/Exe/update.bin
    
    # Check if SD card is mounted
    if [ -d "/Volumes/32GB" ]; then
        echo -e "${YELLOW}Copying update.bin to SD card...${NC}"
        cp Output/Debug/Exe/update.bin /Volumes/32GB/update.bin
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}Firmware copied to SD card!${NC}"
        else
            echo -e "${RED}Failed to copy firmware to SD card${NC}"
        fi
    else
        echo -e "${YELLOW}SD card not mounted at /Volumes/32GB${NC}"
        echo "Firmware available at: CartReaderApp/Output/Debug/Exe/update.bin"
    fi
    
    # Show firmware size
    SIZE=$(ls -lh Output/Debug/Exe/update.bin | awk '{print $5}')
    echo -e "${GREEN}Firmware size: $SIZE${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi