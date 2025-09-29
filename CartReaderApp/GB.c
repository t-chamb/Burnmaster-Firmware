#include <gd32f10x.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Common.h"
#include "Display.h"
#include "Operate.h"
#include "flashparam.h"
#include "fatfs/ff.h"
#include "GBM.h"
#include "GB.h"
#include "soft_uart.h"
#include "GB_Flash.h"

// External functions from GB_Flash.c
extern void writeByte_GB_WithPin(int myAddress, byte myData, enum write_pin pin);
extern const flash_chip_info_t* gb_flash_get_chip_info(uint8_t mfg_id, uint8_t dev_id);


int sramBanks;
int romBanks;
word lastByte = 0;



/******************************************
  Low level functions
*****************************************/
#define dataOut_GB() GPIO_CTL1(DATA) = 0x33333333
//inline void dataOut_GB()
//{
//  //
//  //gpio_init(DATA,GPIO_MODE_OUT_PP,GPIO_OSPEED_50MHZ,BITS(8,15));
//  GPIO_CTL1(DATA) = 0x33333333;
//}
// Switch data pins to read
#define dataIn_GB() GPIO_CTL1(DATA) = 0x44444444
//inline void dataIn_GB()
//{
//  // Set to Input
//  //gpio_init(DATA,GPIO_MODE_IN_FLOATING,GPIO_OSPEED_50MHZ,BITS(8,15));
//  GPIO_CTL1(DATA) = 0x44444444;
//}

void OutAddrBus(word myAddress)
{
  //
  GPIO_OCTL(ADDRLOW) = (GPIO_OCTL(ADDRLOW)&0xFFFF000F) + ((myAddress << 8) & 0xFF00) + ((myAddress >> 8) & 0xF0);
  GPIO_OCTL(ADDRHIGH) = (myAddress & 0x0F00) + (GPIO_OCTL(ADDRHIGH)&0xFFFFF0FF);
}

//#define delay_GB() __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t")

void delay_GB()
{
  //
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");

  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\tnop\n\t");

}

byte readByte_GB(word myAddress) {

  OutAddrBus(myAddress);

  //delay_GB();

  // Switch RD(PH6) to LOW
  gpio_bit_reset(CTRL,RD);

  delay_GB();

  // Read
  byte tempByte = (uint8_t)(GPIO_ISTAT(DATA) >> 8);

  // Switch and RD(PH6) to HIGH
  gpio_bit_set(CTRL,RD);

  //delay_GB();

  return tempByte;
}

void writeByte_GB(int myAddress, byte myData) 
{
  //
  OutAddrBus(myAddress);
  GPIO_OCTL(DATA) = (GPIO_OCTL(DATA)&0xFFFF00FF) + ((myData << 8) & 0xFF00);

  // Arduino running at 16Mhz -> one nop = 62.5ns
  // Wait till output is stable
  delay_GB();

  // Pull WR(PH5) low
  gpio_bit_reset(CTRL,WR);

  // Leave WR low for at least 60ns
  delay_GB();

  // Pull WR(PH5) HIGH
  gpio_bit_set(CTRL,WR);

  // Leave WR high for at least 50ns
  delay_GB();
  delay_GB();
}

// Triggers CS and CLK pin
byte readByteSRAM_GB(word myAddress) {
  OutAddrBus(myAddress);

  delay_GB();

  // Pull CS(PH3) CLK(PH1)(for FRAM MOD) LOW
  gpio_bit_reset(CTRL,CS|CLK);
  // Pull RD(PH6) LOW
  gpio_bit_reset(CTRL,RD);

  delay_GB();

  // Read
  byte tempByte = (uint8_t)(GPIO_ISTAT(DATA) >> 8);

  // Pull RD(PH6) HIGH
  gpio_bit_set(CTRL,RD);
  if (romType == 252) {
    // Pull CS(PH3) HIGH
    gpio_bit_set(CTRL,CS);
  }
  else {
    // Pull CS(PH3) CLK(PH1)(for FRAM MOD) HIGH
    gpio_bit_set(CTRL,CS|CLK);
  }
  delay_GB();

  return tempByte;
}

// Triggers CS and CLK pin
void writeByteSRAM_GB(int myAddress, byte myData) {
  OutAddrBus(myAddress);
  gpio_port_write(DATA,(myData << 8) & 0xFF00);

  delay_GB();

  if (romType == 252) {
    // Pull CS(PH3) LOW
    gpio_bit_reset(CTRL,CS);
    // Pull CLK(PH1)(for GB CAM) HIGH
    gpio_bit_set(CTRL,CLK);
    // Pull WR(PH5) low
    gpio_bit_reset(CTRL,WR);
  }
  else {
    // Pull CS(PH3) CLK(PH1)(for FRAM MOD) LOW
    gpio_bit_reset(CTRL,CS|CLK);
    // Pull WR(PH5) low
    gpio_bit_reset(CTRL,WR);
  }

  // Leave WR low for at least 60ns
  delay_GB();

  if (romType == 252) {
    // Pull WR(PH5) HIGH
    gpio_bit_set(CTRL,WR);
    // Pull CS(PH3) HIGH
    gpio_bit_set(CTRL,CS);
    // Pull  CLK(PH1) LOW (for GB CAM)
    gpio_bit_reset(CTRL,CLK);
  }
  else {
    // Pull WR(PH5) HIGH
    gpio_bit_set(CTRL,WR);
    // Pull CS(PH3) CLK(PH1)(for FRAM MOD) HIGH
    gpio_bit_set(CTRL,CS|CLK);
  }

  // Leave WR high for at least 50ns
  delay_GB();
}


/******************************************
  Game Boy functions
*****************************************/
// Read Cartridge Header
void getCartInfo_GB() 
{
  //
  romType = readByte_GB(0x0147);
  romSize = readByte_GB(0x0148);
  sramSize = readByte_GB(0x0149);

  // ROM banks
  switch (romSize) {
    case 0x00:
      romBanks = 2;
      break;
    case 0x01:
      romBanks = 4;
      break;
    case 0x02:
      romBanks = 8;
      break;
    case 0x03:
      romBanks = 16;
      break;
    case 0x04:
      romBanks = 32;
      break;
    case 0x05:
      romBanks = 64;
      break;
    case 0x06:
      romBanks = 128;
      break;
    case 0x07:
      romBanks = 256;
      break;
    default:
      romBanks = 2;
  }

  // SRAM banks
  sramBanks = 0;
  if (romType == 6) {
    sramBanks = 1;
  }

  // SRAM size
  switch (sramSize) {
    case 2:
      sramBanks = 1;
      break;
    case 3:
      sramBanks = 4;
      break;
    case 4:
      sramBanks = 16;
      break;
    case 5:
      sramBanks = 8;
      break;
  }

  // Last byte of SRAM
  if (romType == 6) {
    lastByte = 0xA1FF;
  }
  if (sramSize == 1) {
    lastByte = 0xA7FF;
  }
  else if (sramSize > 1) {
    lastByte = 0xBFFF;
  }

  // Get Checksum as string
  sprintf(checksumStr, "%02X%02X", readByte_GB(0x014E), readByte_GB(0x014F));

  // Get name
  byte myByte = 0;
  byte myLength = 0;

  for (int addr = 0x0134; addr <= 0x13C; addr++) {
    myByte = readByte_GB(addr);
    if (((myByte >= 48 && myByte <= 57) || (myByte >= 65 && myByte <= 122)) && myLength < 15) {
      romName[myLength] = myByte;
      myLength++;
    }
  }
}




void showCartInfo_GB() 
{
  //
  OledClear();
  if (strcmp((const char *)checksumStr, "00") != 0) {
    OledShowString(0,0,"GB Cart Info:",8);
    OledShowString(2,1,"Name: ",8);
    OledShowString(45,1,romName,8);


    OledShowString(2,2,"Mapper: ",8);
    char * tinfo = NULL;
    if ((romType == 0) || (romType == 8) || (romType == 9))
      tinfo = "none";
    else if ((romType == 1) || (romType == 2) || (romType == 3))
      tinfo = "MBC1";
    else if ((romType == 5) || (romType == 6))
      tinfo = "MBC2";
    else if ((romType == 11) || (romType == 12) || (romType == 13))
      tinfo = "MMM01";
    else if ((romType == 15) || (romType == 16) || (romType == 17) || (romType == 18) || (romType == 19))
      tinfo = "MBC3";
    else if ((romType == 21) || (romType == 22) || (romType == 23))
      tinfo = "MBC4";
    else if ((romType == 25) || (romType == 26) || (romType == 27) || (romType == 28) || (romType == 29) || (romType == 309))
      tinfo = "MBC5";
    if (romType == 252)
      tinfo = "Camera";

    OledShowString(55,2,tinfo,8);

    OledShowString(2,3,"Rom Size: ",8);
    switch (romSize) {
      case 0:
        tinfo = "32KB";
        break;

      case 1:
        tinfo = "64KB";
        break;

      case 2:
        tinfo = "128KB";
        break;

      case 3:
        tinfo = "256KB";
        break;

      case 4:
        tinfo = "512KB";
        break;

      case 5:
        tinfo = "1MB";
        break;

      case 6:
        tinfo = "2MB";
        break;

      case 7:
        tinfo = "4MB";
        break;
    }

    OledShowString(65,3,tinfo,8);
    OledShowString(2,4,"Banks: ",8);
    char tbanks[10] = {0};
    sprintf(tbanks,"%d",romBanks);
    OledShowString(45,4,tbanks,8);

    OledShowString(2,5,"Sram Size: ",8);
    switch (sramSize) {
      case 0:
        if (romType == 6) {
          tinfo = "512B";
        }
        else {
          tinfo = "none";
        }
        break;
      case 1:
        tinfo = "2KB";
        break;

      case 2:
        tinfo = "8KB";
        break;

      case 3:
        tinfo = "32KB";
        break;

      case 4:
        tinfo = "128KB";
        break;

      default: tinfo = "none";
    }
    OledShowString(70,5,tinfo,8);

    OledShowString(2,6,"Checksum: ",8);
    OledShowString(65,6,checksumStr,8);

    // Wait for user input
    OledShowString(0,7,"Press Button...",8);
    WaitOKBtn();
  }
  else {
    OledShowString(0,2,"GAMEPAK ERROR",8);
  }
}


/******************************************
   Setup
 *****************************************/
void setup_GB() {
  
  // Set Address Pins to Output
  



  //A0-A7(D8-D15),A12-A15(D4-D7)
  gpio_init(ADDRLOW,GPIO_MODE_OUT_PP,GPIO_OSPEED_50MHZ,BITS(4,15));
  gpio_port_write(ADDRLOW,0xFFFF);

  //A8-A11
  gpio_init(ADDRHIGH,GPIO_MODE_OUT_PP,GPIO_OSPEED_50MHZ,GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11);
  gpio_bit_set(ADDRHIGH,BITS(8,11));
  


  // Set Control Pins to Output RST(B3) CLK(B12) CS(B15) WR(B13) RD(B14)  
  gpio_init(CTRL,GPIO_MODE_OUT_PP,GPIO_OSPEED_2MHZ,RST|CS|WR|RD|CLK);
  // Output a high signal on all pins, pins are active low therefore everything is disabled now
  gpio_bit_reset(CTRL,RST);
  delay(1);
  gpio_bit_set(CTRL,RST|CS|WR|RD);
  // Output a low signal on CLK to disable writing GB Camera RAM
  gpio_bit_reset(CTRL,CLK);

  // Set Data Pins (D0-D7) to Input
  gpio_init(DATA,GPIO_MODE_IN_FLOATING,GPIO_OSPEED_50MHZ,BITS(8,15));
  // Disable Internal Pullups
  //PORTC = 0x00;

  delay(100);

  // Print start page

  getCartInfo_GB();
  showCartInfo_GB();
}



/******************************************
  ROM functions
*****************************************/
// Read ROM
void readROM_GB() {
  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, romName);
  strcat(fileName, ".GB");

  // create a new folder for the rom file
  foldern = load_dword();
  f_chdir("/");
  sprintf(folder, "GB/ROM/%s/%d", romName, foldern);

  FRESULT rst;
  FIL tfile;

  rst = my_mkdir(folder);
  rst = f_chdir(folder);

  OledClear();
  OledShowString(0,0,"Saving to ",8);
  OledShowString(4,1,folder,8);
  //printf("/..."));

  // write new folder number back to eeprom
  foldern = foldern + 1;
  save_dword(foldern);

  //open file on sd card
  rst = f_open(&tfile,fileName, FA_CREATE_ALWAYS|FA_WRITE);
  if (rst != FR_OK) {
    print_Error("Can't create file", 1);
  }

  word romAddress = 0;

  //Initialize progress bar
  uint32_t processedProgressBar = 0;
  uint32_t totalProgressBar = (uint32_t)(romBanks) * 16384;
  progress_begin("Reading ROM", "Starting...");

  // Read bank 0 first (0x0000-0x3FFF)
  dataIn_GB();
  for (romAddress = 0x0000; romAddress < 0x4000; romAddress += 512) {
    for (int i = 0; i < 512; i++) {
      sdBuffer[i] = readByte_GB(romAddress + i);
    }
    UINT dwt = 0;
    rst = f_write(&tfile, sdBuffer, 512, &dwt);
    processedProgressBar += 512;
    progress_update(processedProgressBar, totalProgressBar);
  }

  // Now read the remaining banks
  for (word currBank = 1; currBank < romBanks; currBank++) {
    // Switch data pins to output
    dataOut_GB();

    LED_BLUE_BLINK;  // Blue for writing

    // Set ROM bank for MBC2/3/4/5
    if (romType >= 5) {
      writeByte_GB(0x2100, currBank);
    }
    // Set ROM bank for MBC1
    else {
      writeByte_GB(0x6000, 0);
      writeByte_GB(0x4000, currBank >> 5);
      writeByte_GB(0x2000, currBank & 0x1F);
    }

    // Switch data pins to input
    dataIn_GB();

    // Banks 1+ are always read from 0x4000-0x7FFF
    romAddress = 0x4000;
    
    // Read banks and save to SD
    while (romAddress <= 0x7FFF) {
      for (int i = 0; i < 512; i++) {
        sdBuffer[i] = readByte_GB(romAddress + i);
      }
      UINT dwt = 0;
      rst = f_write(&tfile,sdBuffer, 512,&dwt);
      romAddress += 512;
      processedProgressBar += 512;
      progress_update(processedProgressBar, totalProgressBar);
    }
  }

  // Close the file:
  f_close(&tfile);
  
  progress_complete("ROM read complete");
}

// Calculate checksum
uint16_t calc_checksum_GB (char* fileName, char* folder) {
  uint16_t calcChecksum = 0;
  //  int calcFilesize = 0; // unused
  unsigned long i = 0;
  int c = 0;
  FIL tfile;
  FRESULT result;
  char msg[128];

  // Save current directory
  f_chdir("/");
  
  if (strcmp(folder, "root") != 0) {
    result = f_chdir(folder);
    if (result != FR_OK) {
      sprintf(msg, "GB Checksum: Failed to chdir to %s (error %d)\r\n", folder, result);
      soft_uart_send_string(msg);
      return 0;
    }
  }

  // If file exists
  sprintf(msg, "GB Checksum: Opening file %s\r\n", fileName);
  soft_uart_send_string(msg);
  
  result = f_open(&tfile, fileName, FA_READ);
  if (result == FR_OK) {
    uint32_t fileSize = f_size(&tfile);
    sprintf(msg, "GB Checksum: File size = %lu bytes\r\n", fileSize);
    soft_uart_send_string(msg);
    
    //calcFilesize = myFile.fileSize() * 8 / 1024 / 1024; // unused
    for (i = 0; i < (fileSize / 512); i++) {
      UINT rdt = 0;
      f_read(&tfile, sdBuffer, 512, &rdt);
      for (c = 0; c < 512; c++) {
        calcChecksum += sdBuffer[c];
      }
    }
    f_close(&tfile);
    
    // Subtract checksum bytes from header
    sprintf(msg, "GB Checksum: Raw sum = %04X\r\n", calcChecksum);
    soft_uart_send_string(msg);

    byte b1 = readByte_GB(0x014E);
    byte b2 = readByte_GB(0x014F);
    sprintf(msg, "GB Checksum: Header bytes = %02X %02X\r\n", b1, b2);
    soft_uart_send_string(msg);
    
    calcChecksum -= b1;
    calcChecksum -= b2;

    // Return result
    return (calcChecksum);
  }
  // Else show error
  else {
    sprintf(msg, "GB Checksum: Can't open file %s (error %d)\r\n", fileName, result);
    soft_uart_send_string(msg);
    print_Error("DUMP ROM 1ST", false);
    return 0;
  }
}

// Compare checksum
boolean compare_checksum_GB() {
  OledShowString(0,3,"Calculating Checksum",8);

  strcpy(fileName, romName);
  strcat(fileName, ".GB");

  // last used rom folder
  foldern = load_dword();
  sprintf(folder, "GB/ROM/%s/%d", romName, foldern - 1);

  // Debug info
  char msg[128];
  sprintf(msg, "GB Checksum: File=%s, Folder=%s\r\n", fileName, folder);
  soft_uart_send_string(msg);

  uint16_t calcChecksum = calc_checksum_GB(fileName, folder);
  
  // Get the expected checksum from cart header as a 16-bit value
  uint16_t expectedChecksum = (readByte_GB(0x014E) << 8) | readByte_GB(0x014F);
  
  char calcsumStr[5];
  char expectedStr[5];
  sprintf(calcsumStr, "%04X", calcChecksum);
  sprintf(expectedStr, "%04X", expectedChecksum);

  sprintf(msg, "GB Checksum: Calc=%04X, Expected=%04X\r\n", calcChecksum, expectedChecksum);
  soft_uart_send_string(msg);

  if (calcChecksum == expectedChecksum) {
    OledShowString(0,4,"Result: ",8);
    OledShowString(50,4,calcsumStr,8);
    OledShowString(0,5,"Checksum matches",8);
    return 1;
  }
  else {
    OledShowString(0,4,"Result: ",8);
    OledShowString(50,4,calcsumStr,8);
    OledShowString(0,5,"Expected: ",8);
    OledShowString(60,5,expectedStr,8);
    print_Error("Checksum Error", false);
    return 0;
  }
}


/******************************************
  SRAM functions
*****************************************/
// Read RAM
void readSRAM_GB() {
  // Does cartridge have RAM
  if (lastByte > 0) {

    // Get name, add extension and convert to char array for sd lib
    strcpy(fileName, romName);
    strcat(fileName, ".sav");

    // create a new folder for the save file
    foldern = load_dword();
    sprintf(folder, "GB/SAVE/%s/%d", romName, foldern);
    my_mkdir(folder);
    f_chdir(folder);

    // write new folder number back to eeprom
    foldern = foldern + 1;
    save_dword(foldern);

    //open file on sd card
    FIL tfile;
    if (f_open(&tfile, fileName, FA_CREATE_ALWAYS|FA_WRITE) != FR_OK) {
      print_Error("SD Error", true);
    }

    dataIn_GB();

    // MBC2 Fix
    readByte_GB(0x0134);

    dataOut_GB();
    if (romType <= 4) {
      writeByte_GB(0x6000, 1);
    }

    // Initialise MBC
    writeByte_GB(0x0000, 0x0A);

    // Switch SRAM banks
    for (byte currBank = 0; currBank < sramBanks; currBank++) {
      dataOut_GB();
      writeByte_GB(0x4000, currBank);

      // Read SRAM
      dataIn_GB();
      for (word sramAddress = 0xA000; sramAddress <= lastByte; sramAddress += 64) {
        for (byte i = 0; i < 64; i++) {
          sdBuffer[i] = readByteSRAM_GB(sramAddress + i);
        }
        UINT wrt;
        f_write(&tfile, sdBuffer, 64, &wrt);
      }
    }

    // Disable SRAM
    dataOut_GB();
    writeByte_GB(0x0000, 0x00);
    dataIn_GB();

    // Close the file:
    f_close(&tfile);

    // Signal end of process
    OledShowString(0,0,"Saved to ",8);
    OledShowString(4,1,folder,8);
    //printf("/"));
  }
  else {
    print_Error("Cart has no SRAM", false);
  }
}

// Write RAM
void writeSRAM_GB() {
  // Does cartridge have SRAM
  if (lastByte > 0) {
    // Create filepath
    //sprintf(filePath, "%s/%s", filePath, fileName);

    //open file on sd card
    FIL tfile;
    if (f_open(&tfile, filePath, FA_READ) == FR_OK) {
      // Set pins to input
      dataIn_GB();

      // MBC2 Fix
      readByte_GB(0x0134);

      dataOut_GB();

      // Enable SRAM for MBC1
      if (romType <= 4) {
        writeByte_GB(0x6000, 1);
      }

      // Initialise MBC
      writeByte_GB(0x0000, 0x0A);

      // Switch RAM banks
      for (byte currBank = 0; currBank < sramBanks; currBank++) {
        writeByte_GB(0x4000, currBank);

        // Write RAM
        for (word sramAddress = 0xA000; sramAddress <= lastByte; sramAddress++) {
          byte bdata;
          UINT rdt = 0;
          f_read(&tfile,&bdata,1,&rdt);
          writeByteSRAM_GB(sramAddress, bdata);
        }
      }
      // Disable SRAM
      writeByte_GB(0x0000, 0x00);

      // Set pins to input
      dataIn_GB();

      // Close the file:
      f_close(&tfile);
      OledClear();
      OledShowString(0,2,"SRAM writing finished",8);

    }
    else {
      print_Error("File doesnt exist", false);
    }
  }
  else {
    print_Error("Cart has no SRAM", false);
  }
}

// Check if the SRAM was written without any error
unsigned long verifySRAM_GB() {

  //open file on sd card
  FIL tfile;
  if (f_open(&tfile,filePath, FA_READ) == FR_OK) {

    // Variable for errors
    writeErrors = 0;

    dataIn_GB();

    // MBC2 Fix
    readByte_GB(0x0134);

    // Check SRAM size
    if (lastByte > 0) {
      dataOut_GB();
      if (romType <= 4) { // MBC1
        writeByte_GB(0x6000, 1); // Set RAM Mode
      }

      // Initialise MBC
      writeByte_GB(0x0000, 0x0A);

      // Switch SRAM banks
      for (byte currBank = 0; currBank < sramBanks; currBank++) {
        dataOut_GB();
        writeByte_GB(0x4000, currBank);

        // Read SRAM
        dataIn_GB();
        for (word sramAddress = 0xA000; sramAddress <= lastByte; sramAddress += 64) {
          //fill sdBuffer
          UINT rdt;
          f_read(&tfile, sdBuffer, 64, &rdt);
          for (int c = 0; c < 64; c++) {
            if (readByteSRAM_GB(sramAddress + c) != sdBuffer[c]) {
              writeErrors++;
            }
          }
        }
      }
      dataOut_GB();
      // Disable RAM
      writeByte_GB(0x0000, 0x00);
      dataIn_GB();
    }
    // Close the file:
    f_close(&tfile);
    return writeErrors;
  }
  else {
    print_Error("Can't open file", true);
  }
  return 0;
}

//检测sram
void TestSramGB(byte bankCnt , word wTestSize)
{

  OledClear();
  OledShowString(0,6,"start RAM testing...",8);
  //
  // Set pins to input
  dataIn_GB();

  // MBC2 Fix
  readByte_GB(0x0134);

  dataOut_GB();

  // Enable SRAM for MBC1
  if (romType <= 4) {
    writeByte_GB(0x6000, 1);
  }

  // Initialise MBC
  writeByte_GB(0x0000, 0x0A);

  // Switch RAM banks
  for (byte currBank = 0; currBank < bankCnt; currBank++) {
    writeByte_GB(0x4000, currBank);

    LED_BLUE_BLINK;  // Blue for writing
    // Write RAM
    for (word sramAddress = 0xA000; sramAddress <= wTestSize; sramAddress++) {
      byte bdata = sramAddress & 0xFF;
      writeByteSRAM_GB(sramAddress, bdata);      
    }
  }
  // Disable SRAM
  writeByte_GB(0x0000, 0x00);

  // Set pins to input
  dataIn_GB();



  
  // Variable for errors
  int32_t wErrors = 0;

  // MBC2 Fix
  readByte_GB(0x0134);

  // Check SRAM size
  dataOut_GB();
  if (romType <= 4) { // MBC1
    writeByte_GB(0x6000, 1); // Set RAM Mode
  }

  // Initialise MBC
  writeByte_GB(0x0000, 0x0A);

  // Switch SRAM banks
  for (byte currBank = 0; currBank < bankCnt; currBank++) {
    dataOut_GB();
    writeByte_GB(0x4000, currBank);

    LED_RED_BLINK;  // Red for verifying
    // Read SRAM
    dataIn_GB();
    for (word sramAddress = 0xA000; sramAddress <= wTestSize; sramAddress++) {
        byte bdata = sramAddress & 0xFF;
        if (readByteSRAM_GB(sramAddress) != bdata) {
          wErrors++;
        }
    }
  }
  dataOut_GB();
  // Disable RAM
  writeByte_GB(0x0000, 0x00);
  dataIn_GB();

  char msgbuf[64] = {0};
  if(wErrors > 0){
    //
    sprintf(msgbuf,"Error %d bytes...",wErrors);
  }else{
    //
    strcpy(msgbuf,"RAM Test ok!");
  }
  OledShowString(0,1,msgbuf,8);
}


/******************************************
  29F016/29F032/29F033 flashrom functions
*****************************************/
// DEPRECATED: Use writeCFI_GB() instead which uses the generic flash database
// This old function has hardcoded commands and delays
// Write 29F032 flashrom
// A0-A13 directly connected to cart edge -> 16384(0x0-0x3FFF) bytes per bank -> 256(0x0-0xFF) banks
// A14-A21 connected to MBC5
void writeFlash29F_GB(byte MBC, boolean flashErase) {
  // Launch filebrowser
  filePath[0] = '\0';
  f_chdir("/");
  fileBrowser("/","Select file:");
  OledClear();

  FIL tf;
  UINT rdt;
  uint16_t wfid;
  char msgbuf[64] = {0};

  // Open file on sd card
  if (f_open(&tf,filePath, FA_READ) == FR_OK) 
  {
    // Get rom size from file
    f_lseek(&tf,0x147);
    f_read(&tf,&romType,1,&rdt);
    f_read(&tf,&romSize,1,&rdt);
    // Go back to file beginning
    f_lseek(&tf,0);

    // ROM banks
    if(romSize < 8)
      romBanks = 1 << (romSize + 1);
    else 
      romBanks = 2;

    // Set data pins to output
    dataOut_GB();

    // Set ROM bank hi 0
    writeByte_GB(0x3000, 0);
    // Set ROM bank low 0
    writeByte_GB(0x2000, 0);
    delay(100);

    // Reset flash
    writeByte_GB(0x555, 0xf0);
    delay(100);

    // ID command sequence
    writeByte_GB(0x555, 0xaa);
    delay(1);
    writeByte_GB(0x2aa, 0x55);
    delay(1);
    writeByte_GB(0x555, 0x90);
    delay(1);

    dataIn_GB();

    // Read the two id bytes into a string
    wfid = readByte_GB(0);
    wfid = (wfid << 8)&0xFF00;
    wfid += readByte_GB(1)&0xFF;
    sprintf(flashid, "%04X", wfid);

    if (wfid == 0x04d4) {
      sprintf(msgbuf,"MBM29F033C\nBanks: %d/256",romBanks);
    }
    else if (wfid == 0x0141) {
      sprintf(msgbuf,"AM29F032B\nBanks: %d/256",romBanks);
    }
    else if (wfid == 0x01AD) {
      sprintf(msgbuf,"AM29F016B\nBanks: %d/256",romBanks);
    }
    else if (wfid == 0x04AD) {
      sprintf(msgbuf,"AM29F016D\nBanks: %d/256",romBanks);
    }
    else if (wfid == 0x01D5) {
      sprintf(msgbuf,"AM29F080B\nBanks: %d/256",romBanks);
    }
    else {
      
      OledShowString(0,0,"Flash ID: ",8);
      OledShowString(60,0,flashid,8);
      f_close(&tf);
      print_Error("Unknown flashrom", true);
    }

    //
    OledShowString(0,0,msgbuf,8);

    dataOut_GB();

    // Reset flash
    writeByte_GB(0x555, 0xf0);
    delay(100);

    if (flashErase) 
    {
      // Use progress system for erasing
      progress_begin("Erasing Flash", "Please wait...");

      // Erase flash
      writeByte_GB(0x555, 0xaa);
      writeByte_GB(0x2aa, 0x55);
      writeByte_GB(0x555, 0x80);
      writeByte_GB(0x555, 0xaa);
      writeByte_GB(0x2aa, 0x55);
      writeByte_GB(0x555, 0x10);

      // Set data pins to input
      dataIn_GB();
      // Read the status register
      byte statusReg = readByte_GB(0);
      // After a completed erase D7 will output 1
      while ((statusReg & 0x80) != 0x80) {
        // Update Status
        statusReg = readByte_GB(0);
      }

      // Blankcheck - removed conflicting OledShowString since progress bar shows status

      // Read x number of banks
      for (int currBank = 0; currBank < romBanks; currBank++) 
      {
        // Blink led
        LED_BLUE_BLINK;  // Blue for writing

        dataOut_GB();

        // Set ROM bank
        writeByte_GB(0x2000, currBank);
        dataIn_GB();

        for (unsigned int currAddr = 0x4000; currAddr < 0x7FFF; currAddr += 512) {
          for (int currByte = 0; currByte < 512; currByte++) {
            sdBuffer[currByte] = readByte_GB(currAddr + currByte);
          }
          for (int j = 0; j < 512; j++) {
            if (sdBuffer[j] != 0xFF) {
              OledShowString(0,5,"Not empty",8);
              f_close(&tf);
              print_Error("Erase failed!", true);
            }
          }
        }
      }
    }



    if (MBC == 3) {
      // Writing flash MBC3 - status shown in progress bar

      // Write flash
      dataOut_GB();

      word currAddr = 0;
      word endAddr = 0x3FFF;

      //Initialize progress bar
      uint32_t processedProgressBar = 0;
      uint32_t totalProgressBar = (uint32_t)(romBanks) * 16384;
      progress_begin("Writing MBC3", "Starting...");

      for (int currBank = 0; currBank < romBanks; currBank++) {
        // Blink led
        LED_BLUE_BLINK;  // Blue for writing

        // Set ROM bank
        writeByte_GB(0x2100, currBank);
        
        char bankStatus[32];
        sprintf(bankStatus, "Writing MBC3 bank %d/%d", currBank + 1, romBanks);
        progress_set_status(bankStatus);

        if (currBank > 0) {
          currAddr = 0x4000;
          endAddr = 0x7FFF;
        }

        while (currAddr <= endAddr) {
          f_read(&tf,sdBuffer, 512,&rdt);
          for (int currByte = 0; currByte < 512; currByte++) 
          {
            // Write command sequence
            writeByte_GB(0x555, 0xaa);
            writeByte_GB(0x2aa, 0x55);
            writeByte_GB(0x555, 0xa0);
            // Write current byte
            writeByte_GB(currAddr + currByte, sdBuffer[currByte]);

            // Set data pins to input
            dataIn_GB();

            // Set OE/RD(PH6) LOW
            gpio_bit_reset(CTRL,RD);
            //PORTH &= ~(1 << 6);

            // Busy check
            while (((GPIO_ISTAT(DATA) >> 8) & 0x80) != (sdBuffer[currByte] & 0x80)) {
            }

            // Switch OE/RD(PH6) to HIGH
            gpio_bit_set(CTRL,RD);
            //PORTH |= (1 << 6);

            // Set data pins to output
            dataOut_GB();
          }
          currAddr += 512;
          processedProgressBar += 512;
          progress_update(processedProgressBar, totalProgressBar);
        }
      }
      
      progress_complete("MBC3 write complete");
    }

    else if (MBC == 5) 
    {
      // Writing flash MBC5 - status shown in progress bar

      // Write flash
      dataOut_GB();

      //Initialize progress bar
      uint32_t processedProgressBar = 0;
      uint32_t totalProgressBar = (uint32_t)(romBanks) * 16384;
      progress_begin("Writing MBC5", "Starting...");

      for (int currBank = 0; currBank < romBanks; currBank++) {
        // Blink led
        LED_BLUE_BLINK;  // Blue for writing

        // Set ROM bank
        writeByte_GB(0x2000, currBank);
        // 0x2A8000 fix
        writeByte_GB(0x4000, 0x0);
        
        char bankStatus[32];
        sprintf(bankStatus, "Writing MBC5 bank %d/%d", currBank + 1, romBanks);
        progress_set_status(bankStatus);

        for (unsigned int currAddr = 0x4000; currAddr < 0x7FFF; currAddr += 512) 
        {
          f_read(&tf,sdBuffer, 512,&rdt);

          for (int currByte = 0; currByte < 512; currByte++) {
            // Write command sequence
            writeByte_GB(0x555, 0xaa);
            writeByte_GB(0x2aa, 0x55);
            writeByte_GB(0x555, 0xa0);
            // Write current byte
            writeByte_GB(currAddr + currByte, sdBuffer[currByte]);

            // Set data pins to input
            dataIn_GB();

            // Set OE/RD(PH6) LOW
            gpio_bit_reset(CTRL,RD);

            // Busy check
            while (((GPIO_ISTAT(DATA) >> 8) & 0x80) != (sdBuffer[currByte] & 0x80)) {

            }

            // Switch OE/RD(PH6) to HIGH
            gpio_bit_set(CTRL,RD);

            // Set data pins to output
            dataOut_GB();
          }
          processedProgressBar += 512;
          progress_update(processedProgressBar, totalProgressBar);
        }
      }
      
      progress_complete("MBC5 write complete");
    }

    // Set data pins to input again
    dataIn_GB();

    // Start verification with progress system
    progress_begin("Verifying ROM", "Bank 1");

    // Go back to file beginning
    f_lseek(&tf,0);
    //unsigned int addr = 0;  // unused
    writeErrors = 0;
    // Verify flashrom
    word romAddress = 0;

    // Read number of banks and switch banks
    for (word bank = 1; bank < romBanks; bank++) {
      // Switch data pins to output
      dataOut_GB();

      if (romType >= 5) { // MBC2 and above
        writeByte_GB_WithPin(0x2100, bank, WRITE_PIN_WR); // Set ROM bank
      }
      else { // MBC1
        writeByte_GB_WithPin(0x6000, 0, WRITE_PIN_WR); // Set ROM Mode
        writeByte_GB_WithPin(0x4000, bank >> 5, WRITE_PIN_WR); // Set bits 5 & 6 (01100000) of ROM bank
        writeByte_GB_WithPin(0x2000, bank & 0x1F, WRITE_PIN_WR); // Set bits 0 & 4 (00011111) of ROM bank
      }

      // Switch data pins to intput
      dataIn_GB();

      if (bank > 1) {
        romAddress = 0x4000;
      }
      // Blink led
      LED_BLUE_BLINK;  // Blue for writing
      
      // Update progress
      progress_update(bank - 1, romBanks - 1);
      char bankStatus[32];
      sprintf(bankStatus, "Bank %d", bank);
      progress_set_status(bankStatus);

      // Read up to 7FFF per bank
      while (romAddress <= 0x7FFF) {
        // Fill sdBuffer
        f_read(&tf,sdBuffer, 512,&rdt);
        // Compare
        for (int i = 0; i < 512; i++) {
          if (readByte_GB(romAddress + i) != sdBuffer[i]) {
            writeErrors++;
          }
        }
        romAddress += 512;
      }
    }
    
    progress_update(romBanks - 1, romBanks - 1);
    // Close the file:
    f_close(&tf);

    if (writeErrors == 0) {
      progress_complete("Verified OK!");
    }
    else {
      char err_msg[32];
      sprintf(err_msg, "Error:%d bytes", writeErrors);
      progress_complete(err_msg);
      print_Error("Did not verify :(", true);
    }
  }
  else {
    OledShowString(0,0,"Can't open file!",8);
  }
}




/******************************************
  CFU flashrom functions
*****************************************/

bool flashX16Mode;
bool flashSwitchLastBits;
unsigned long flashBanks;
uint8_t flash_mfg_id = 0;
uint8_t flash_dev_id = 0;

/*
   Flash chips can either be in x8 mode or x16 mode and sometimes the two
   least significant bits on flash cartridges' data lines are swapped.
   This function reads a byte and compensates for the differences.
   This is only necessary for commands to the flash, not for data read from the flash, the MBC or SRAM.

   address needs to be the x8 mode address of the flash register that should be read.
*/
// DEPRECATED: Use gb_flash_read_byte_compensated instead
byte readByteCompensated(int address) {
  return gb_flash_read_byte_compensated(address, flashX16Mode, flashSwitchLastBits);
}

/*
   Flash chips can either be in x8 mode or x16 mode and sometimes the two
   least significant bits on flash cartridges' data lines are swapped.
   This function writes a byte and compensates for the differences.
   This is only necessary for commands to the flash, not for data written to the flash, the MBC or SRAM.
   .
   address needs to be the x8 mode address of the flash register that should be read.
*/
// DEPRECATED: Use gb_flash_write_byte_compensated instead
void writeByteCompensated(int address, byte data) {
  gb_flash_write_byte_compensated(address, data, flashX16Mode, flashSwitchLastBits, WRITE_PIN_WR);
}

// DEPRECATED: Use gb_flash_write_byte_compensated instead
void writeByteCompensatedWithPin(int address, byte data, enum write_pin pin) {
  gb_flash_write_byte_compensated(address, data, flashX16Mode, flashSwitchLastBits, pin);
}

// DEPRECATED: Use gb_flash_enter_cfi_mode instead
void startCFIMode(boolean x16Mode) {
  gb_flash_enter_cfi_mode(x16Mode, WRITE_PIN_WR);
}

// DEPRECATED: Use gb_flash_enter_cfi_mode instead 
void startCFIModeWithPin(boolean x16Mode, enum write_pin pin) {
  gb_flash_enter_cfi_mode(x16Mode, pin);
}

/* Identify the different flash chips.
   Sets the global variables flashBanks, flashX16Mode and flashSwitchLastBits
   This function now uses the generic flash identification from GB_Flash.c
*/
void identifyCFI_GB() 
{
  soft_uart_send_string("GB Flash: identifyCFI_GB() called\r\n");
  
  // Clear OLED display
  OledClear();
  
  // Initialize flash detection
  dataOut_GB();
  
  // Set initial bank configuration
  writeByte_GB_WithPin(0x6000, 0, WRITE_PIN_WR); // Set ROM Mode
  writeByte_GB_WithPin(0x2000, 0, WRITE_PIN_WR); // Set Bank to 0  
  writeByte_GB_WithPin(0x3000, 0, WRITE_PIN_WR);
  
  // Use generic flash identification
  uint8_t mfg_id = 0, dev_id = 0;
  enum flash_type chip_type;
  bool x16_mode = false, switch_bits = false;
  uint16_t banks = 0;
  
  // Call the generic identification function
  bool identified = gb_flash_identify_chip(&mfg_id, &dev_id, &chip_type, 
                                          &x16_mode, &switch_bits, &banks);
  
  if (!identified) {
    soft_uart_send_string("GB Flash: Failed to identify flash chip!\r\n");
    OledShowString(0, 2, "CFI Query failed!", 8);
    OledShowString(0, 3, "Unknown chip", 8);
    WaitOKBtn();
    return;
  }
  
  // Update global variables
  flash_mfg_id = mfg_id;
  flash_dev_id = dev_id;
  flashX16Mode = x16_mode;
  flashSwitchLastBits = switch_bits;
  flashBanks = banks;
  
  // Get chip info for display
  const flash_chip_info_t* chip_info = gb_flash_get_chip_info(mfg_id, dev_id);
  
  if (chip_info) {
    soft_uart_send_string("GB Flash: Chip found in database\r\n");
    char msg[128];
    sprintf(msg, "GB Flash: %s (%02X:%02X)\r\n", chip_info->name, mfg_id, dev_id);
    soft_uart_send_string(msg);
    
    // Use the new cart requirements check function from GB_Flash.c
    if (!gb_flash_check_cart_requirements(mfg_id, dev_id, banks)) {
        // Cart requirements failed, display appropriate warning
        if (mfg_id == 0xF3 && dev_id == 0xC3) {
            // FunnyPlaying RTC cart switch warning
            OledClear();
            OledShowString(0, 0, "SWITCH POSITION!", 8);
            OledShowString(0, 1, "FunnyPlaying RTC", 8);
            OledShowString(0, 2, "cart detected.", 8);
            OledShowString(0, 3, "Please set switch", 8);
            OledShowString(0, 4, "to FLASH position", 8);
            OledShowString(0, 5, "then press OK", 8);
            WaitOKBtn();
        } else if (mfg_id == 0x01 && dev_id == 0x01) {
            // MidnightTrace MBC mode warning
            OledClear();
            OledShowString(0, 0, "MBC MODE ERROR!", 8);
            OledShowString(0, 1, "MidnightTrace cart", 8);
            OledShowString(0, 2, "must be in MBC5", 8);
            OledShowString(0, 3, "mode to flash.", 8);
            OledShowString(0, 4, "", 8);
            OledShowString(0, 5, "Set to MBC5 mode", 8);
            OledShowString(0, 6, "then press OK", 8);
            WaitOKBtn();
        }
        
        // Re-identify after user has made changes
        soft_uart_send_string("GB Flash: Re-identifying after user changes\r\n");
        identified = gb_flash_identify_chip(&mfg_id, &dev_id, &chip_type, 
                                          &x16_mode, &switch_bits, &banks);
        if (!identified) {
            soft_uart_send_string("GB Flash: Failed to re-identify\r\n");
            OledShowString(0, 6, "Still can't ID chip", 8);
            OledShowString(0, 7, "Check cart setup!", 8);
            WaitOKBtn();
            return;
        }
        
        // Update chip info
        chip_info = gb_flash_get_chip_info(mfg_id, dev_id);
        
        // Re-check requirements
        if (!gb_flash_check_cart_requirements(mfg_id, dev_id, banks)) {
            soft_uart_send_string("GB Flash: Cart requirements still not met\r\n");
            OledShowString(0, 6, "Requirements not met", 8);
            OledShowString(0, 7, "Cannot continue!", 8);
            WaitOKBtn();
            return;
        }
    }
    
    // Display on OLED
    OledShowString(0, 2, chip_info->name, 8);
    
    // Display additional info
    char info_buf[64];
    sprintf(info_buf, "Banks: %d", flashBanks);
    OledShowString(0, 3, info_buf, 8);
    sprintf(info_buf, "Mode: %s %s", 
            x16_mode ? "x16" : "x8",
            switch_bits ? "swapped" : "normal");
    OledShowString(0, 4, info_buf, 8);
  } else {
    char msg[64];
    sprintf(msg, "Unknown chip: %02X:%02X", mfg_id, dev_id);
    OledShowString(0, 2, msg, 8);
    sprintf(msg, "Banks: %d", flashBanks);  
    OledShowString(0, 3, msg, 8);
  }
  
  // Brief delay to show info
  delay(1000);
  
  soft_uart_send_string("GB Flash: Identification complete\r\n");
}

// Progress callback helper for flash operations
static void gb_progress_handler(const char* status, int current, int total) {
  progress_update(current, total);
  if (status) {
    progress_set_status(status);
  }
}

// Write flash using generic flash functions from GB_Flash.c
// This function uses the chip database and proper timing from chip info
bool writeCFI_GB() {
  soft_uart_send_string("GB Flash: writeCFI_GB() started\r\n");
  
  FIL tf;
  UINT rdt;
  char msgbuf[128] = {0};
  uint32_t use_tick = getSystick();
  
  // Get chip info from detected IDs
  const flash_chip_info_t* chip_info = NULL;
  if (flash_mfg_id != 0 && flash_dev_id != 0) {
    chip_info = gb_flash_get_chip_info(flash_mfg_id, flash_dev_id);
    if (chip_info) {
      char chip_msg[128];
      sprintf(chip_msg, "GB Flash: Using chip database for %s\r\n", chip_info->name);
      soft_uart_send_string(chip_msg);
      sprintf(chip_msg, "GB Flash: chip_info->commands=%p\r\n", chip_info->commands);
      soft_uart_send_string(chip_msg);
    } else {
      soft_uart_send_string("GB Flash: ERROR - No chip info found for detected IDs\r\n");
      OledShowString(0,1,"Unknown flash chip!",8);
      return false;
    }
  } else {
    soft_uart_send_string("GB Flash: ERROR - No flash chip detected\r\n");
    OledShowString(0,1,"No flash detected!",8);
    return false;
  }
  
  // Debug file path
  char path_msg[128];
  sprintf(path_msg, "GB Flash: Opening file [%s]\r\n", filePath);
  soft_uart_send_string(path_msg);
  
  // Open file on sd card
  if (f_open(&tf, filePath, FA_READ) != FR_OK) {
    soft_uart_send_string("GB Flash: writeCFI_GB() - Can't open file!\r\n");
    OledShowString(0,1,"Can't open file!",8);
    return false;
  }
  
  soft_uart_send_string("GB Flash: File opened successfully\r\n");
  
  // Get rom size from file header
  f_lseek(&tf, 0x147);
  f_read(&tf, &romType, 1, &rdt);
  f_read(&tf, &romSize, 1, &rdt);
  
  char rom_info[64];
  sprintf(rom_info, "GB Flash: ROM type=0x%02X, size=0x%02X\r\n", romType, romSize);
  soft_uart_send_string(rom_info);
  
  // Calculate ROM banks
  if (romSize < 8) {
    romBanks = 1 << (romSize + 1);
  } else {
    romBanks = 2;
  }
  
  sprintf(msgbuf, "GB Flash: ROM has %d banks, Flash has %d banks\r\n", romBanks, flashBanks);
  soft_uart_send_string(msgbuf);
  
  // Check if flash has enough banks
  if (romBanks > flashBanks) {
    soft_uart_send_string("GB Flash: ERROR - Flash has too few banks!\r\n");
    sprintf(msgbuf,"Error:\nFlash has too few banks!\nHas %d\nbut needs %d banks.", flashBanks, romBanks);
    OledShowString(0,0,msgbuf,8);
    OledShowString(0,7,"Press OK button...",8);
    f_close(&tf);
    WaitOKBtn();
    return false;
  }
  
  // Display bank usage
  sprintf(msgbuf,"Using %d/%d Banks", romBanks, flashBanks);
  OledShowString(0,0,msgbuf,8);
  soft_uart_send_string("GB Flash: Bank check passed, proceeding with operation\r\n");
  
  // Read entire ROM file into sdBuffer
  // Note: This assumes the file fits in available memory
  uint32_t rom_size = romBanks * 0x4000;
  f_lseek(&tf, 0);
  
  // Allocate memory for ROM data (if needed)
  // For now, we'll read and write in chunks
  
  // Exit any CFI mode first
  soft_uart_send_string("GB Flash: Exiting CFI mode\r\n");
  gb_flash_exit_cfi_mode(chip_info);
  
  // Reset to bank 0
  gb_flash_switch_bank(0);
  delay(10);
  
  // Reset flash chip
  gb_flash_reset_with_pin(chip_info->write_pin);
  delay(100);
  
  // 1. ERASE PHASE
  soft_uart_send_string("GB Flash: Starting erase procedure\r\n");
  progress_begin("Flash Operation", "Erasing...");
  
  bool erase_result = gb_flash_erase_with_progress(chip_info, romBanks, gb_progress_handler);
  
  if (!erase_result) {
    soft_uart_send_string("GB Flash: Erase failed!\r\n");
    progress_complete("Erase failed!");
    f_close(&tf);
    return false;
  }
  
  progress_complete("Erase complete");
  
  // 2. BLANK CHECK PHASE
  soft_uart_send_string("GB Flash: Starting blank check\r\n");
  progress_begin("Flash Operation", "Blank check...");
  
  dataIn_GB();
  bool blank_check_pass = true;
  
  for (int currBank = 0; currBank < romBanks; currBank++) {
    progress_update(currBank, romBanks);
    
    char bankStatus[32];
    sprintf(bankStatus, "Checking bank %d/%d", currBank + 1, romBanks);
    progress_set_status(bankStatus);
    
    // Set bank
    dataOut_GB();
    gb_flash_switch_bank(currBank);
    dataIn_GB();
    
    // Check appropriate range
    uint16_t start_addr = (currBank == 0) ? 0x0000 : 0x4000;
    uint16_t end_addr = (currBank == 0) ? 0x3FFF : 0x7FFF;
    
    for (uint16_t addr = start_addr; addr <= end_addr; addr += 512) {
      // Read 512 bytes at a time
      for (int i = 0; i < 512 && (addr + i) <= end_addr; i++) {
        if (readByte_GB(addr + i) != 0xFF) {
          char err_msg[64];
          sprintf(err_msg, "Not blank at bank %d, addr 0x%04X\r\n", currBank, addr + i);
          soft_uart_send_string(err_msg);
          blank_check_pass = false;
          break;
        }
      }
      if (!blank_check_pass) break;
    }
    if (!blank_check_pass) break;
    
    LED_BLUE_BLINK;  // Blue for writing
  }
  
  progress_update(romBanks, romBanks);
  
  if (!blank_check_pass) {
    progress_complete("Blank check failed!");
    OledShowString(0,6,"Not empty",8);
    f_close(&tf);
    print_Error("Erase failed", true);
    return false;
  }
  
  progress_complete("Blank check complete");
  
  // 3. WRITE PHASE
  soft_uart_send_string("GB Flash: Starting write operation\r\n");
  progress_begin("Flash Operation", "Writing...");
  
  // Reset file position
  f_lseek(&tf, 0);
  
  // Set data pins to output
  dataOut_GB();
  
  bool write_success = true;
  
  for (int currBank = 0; currBank < romBanks; currBank++) {
    progress_update(currBank, romBanks);
    
    char bankStatus[32];
    sprintf(bankStatus, "Writing bank %d/%d", currBank + 1, romBanks);
    progress_set_status(bankStatus);
    
    // Set bank
    gb_flash_switch_bank(currBank);
    delay(1);
    
    // Determine address range
    uint16_t start_addr = (currBank == 0) ? 0x0000 : 0x4000;
    uint16_t end_addr = (currBank == 0) ? 0x3FFF : 0x7FFF;
    
    // Write bank data
    for (uint16_t addr = start_addr; addr <= end_addr; addr += 512) {
      // Read 512 bytes from file
      f_read(&tf, sdBuffer, 512, &rdt);
      
      // Write bytes
      for (int i = 0; i < 512 && (addr + i) <= end_addr; i++) {
        if (!gb_flash_write_byte_new(chip_info, currBank, addr + i, sdBuffer[i])) {
          char err_msg[128];
          sprintf(err_msg, "GB Flash: Write failed at bank %d, addr 0x%04X\r\n", 
                  currBank, addr + i);
          soft_uart_send_string(err_msg);
          write_success = false;
          break;
        }
      }
      
      if (!write_success) break;
      
      // Blink LED more frequently during write
      if ((addr & 0x1FF) == 0) {  // Every 512 bytes
        LED_BLUE_BLINK;  // Blue for writing
        
        // Update progress more frequently
        uint32_t current_progress = (currBank * 0x4000) + (addr - start_addr);
        progress_update(current_progress, rom_size);
      }
    }
    
    if (!write_success) break;
  }
  
  progress_update(romBanks, romBanks);
  
  if (!write_success) {
    progress_complete("Write failed!");
    f_close(&tf);
    return false;
  }
  
  progress_complete("Write complete");
  
  // 4. VERIFY PHASE
  soft_uart_send_string("GB Flash: Starting verification\r\n");
  progress_begin("Verifying Flash", "Bank 0");
  
  // Reset file position
  f_lseek(&tf, 0);
  dataIn_GB();
  
  writeErrors = 0;
  
  for (word bank = 0; bank < romBanks; bank++) {
    // Switch data pins to output for bank switching
    dataOut_GB();
    
    if (romType >= 5) { // MBC2 and above
      writeByte_GB_WithPin(0x2100, bank, WRITE_PIN_WR); // Set ROM bank
    } else { // MBC1
      writeByte_GB_WithPin(0x6000, 0, WRITE_PIN_WR); // Set ROM Mode
      writeByte_GB_WithPin(0x4000, bank >> 5, WRITE_PIN_WR); // Set bits 5 & 6
      writeByte_GB_WithPin(0x2000, bank & 0x1F, WRITE_PIN_WR); // Set bits 0-4
    }
    
    // Switch data pins back to input
    dataIn_GB();
    
    // Update progress
    char bank_status[32];
    sprintf(bank_status, "Bank %d", bank);
    progress_set_status(bank_status);
    progress_update(bank, romBanks);
    
    // Verify appropriate range
    uint16_t start_addr = (bank == 0) ? 0x0000 : 0x4000;
    uint16_t end_addr = (bank == 0) ? 0x3FFF : 0x7FFF;
    
    for (uint16_t addr = start_addr; addr <= end_addr; addr += 512) {
      // Read expected data from file
      f_read(&tf, sdBuffer, 512, &rdt);
      
      // Compare with flash
      for (int i = 0; i < 512 && (addr + i) <= end_addr; i++) {
        byte flashByte = readByte_GB(addr + i);
        if (flashByte != sdBuffer[i]) {
          writeErrors++;
          // Report first few errors
          if (writeErrors <= 10) {
            char err_msg[128];
            sprintf(err_msg, "GB Flash: Verify error at Bank %d, Addr %04X: expected %02X, got %02X\r\n",
                    bank, addr + i, sdBuffer[i], flashByte);
            soft_uart_send_string(err_msg);
          }
        }
      }
    }
    
    LED_BLUE_BLINK;  // Blue for writing
  }
  
  progress_update(romBanks, romBanks);
  
  // Close file
  f_close(&tf);
  
  // Report results
  if (writeErrors == 0) {
    progress_complete("Verified OK!");
    use_tick = (getSystick() - use_tick) / 1055;
    sprintf(msgbuf, "Use Time: %lu(s)", use_tick);
    OledShowString(10, 6, msgbuf, 8);
    soft_uart_send_string("GB Flash: Flash operation SUCCESS!\r\n");
    soft_uart_send_string("GB Flash: writeCFI_GB returning true\r\n");
    return true;
  } else {
    char err_msg[32];
    sprintf(err_msg, "Error:%lu bytes", writeErrors);
    progress_complete(err_msg);
    
    char uart_msg[64];
    sprintf(uart_msg, "GB Flash: Verification failed with %lu errors\r\n", writeErrors);
    soft_uart_send_string(uart_msg);
    
    print_Error("Did not verify...", false);
    return false;
  }
}



void testCFI_GB(uint16_t testBanks) {
  //
  OledShowString(0,6,"Start ROM Testing...",8);
  // Set data pins to output
  dataOut_GB();

  // Set ROM bank hi 0
  writeByte_GB_WithPin(0x3000, 0, WRITE_PIN_AUDIO);
  // Set ROM bank low 0
  writeByte_GB_WithPin(0x2000, 0, WRITE_PIN_AUDIO);
  delay(100);

  // Reset flash
  writeByteCompensatedWithPin(0xAAA, 0xf0, WRITE_PIN_WR);
  delay(100);
  dataOut_GB();
  // Reset flash
  writeByte_GB_WithPin(0x555, 0xf0, WRITE_PIN_AUDIO);
  delay(100);


  // Erase flash   
  int lastSector = (testBanks << 1);
  printf("lastSector=%d\n",lastSector);
  progress_begin("Erasing Test", "Starting...");
  for (int currSector = 0x0; currSector < lastSector; currSector++)
  {
      //
      int SA = ((currSector >> 1)?0x4000:0) + (currSector & 0x01)*0x2000;
      dataOut_GB();
      //writeByte_GB_WithPin(0x2000, 0, WRITE_PIN_AUDIO);
      writeByte_GB_WithPin(0x2100, currSector >> 1, WRITE_PIN_AUDIO);
      delayMicroseconds(1); 
      
            
      writeByteCompensatedWithPin(0xAAA, 0xAA, WRITE_PIN_WR);
      //delayMicroseconds(1);   
      writeByteCompensatedWithPin(0x555, 0x55, WRITE_PIN_WR);
      //delayMicroseconds(1);   
      writeByteCompensatedWithPin(0xAAA, 0x80, WRITE_PIN_WR);
      //delayMicroseconds(1);   
      writeByteCompensatedWithPin(0xAAA, 0xAA, WRITE_PIN_WR);
      //delayMicroseconds(1);   
      writeByteCompensatedWithPin(0x555, 0x55, WRITE_PIN_WR);

      //writeByte_GB_WithPin(0x2000, currSector >> 1, WRITE_PIN_AUDIO);
      //delayMicroseconds(1); 
      writeByteCompensatedWithPin(SA, 0x30, WRITE_PIN_WR);
      //delay(50);

      // Blink LED
      LED_RED_BLINK;  // Red for erasing
      
      char sectorStatus[32];
      sprintf(sectorStatus, "Erasing sector %d/%d", currSector + 1, lastSector);
      progress_set_status(sectorStatus);
      
      progress_update(currSector, lastSector);

      //
      dataIn_GB();
      // Read the status register
      byte statusReg = readByte_GB(SA);
      printf("curSector = %d,SA=0x%04x\n",currSector,SA);

      // After a completed erase D7 will output 1
      while ((statusReg | 0x7F) != 0xFF) {
        // Blink led
        delay(5);
        // Update Status
        statusReg = readByte_GB(SA);
      }      
  }
  progress_update(lastSector, lastSector);
  progress_complete("Test erase complete");


  // Writing... - status shown in progress bar
  // Write flash
  // Set data pins to output
  dataOut_GB();
  // Set ROM bank hi 0
  writeByte_GB_WithPin(0x3000, 0, WRITE_PIN_AUDIO);
  // Set ROM bank low 0
  writeByte_GB_WithPin(0x2000, 0, WRITE_PIN_AUDIO);
  delay(100);
  // Reset flash
  writeByteCompensatedWithPin(0xAAA, 0xf0, WRITE_PIN_WR);
  delay(100);
  dataOut_GB();
  // Reset flash
  writeByte_GB_WithPin(0x555, 0xf0, WRITE_PIN_AUDIO);
  delay(100);



  word currAddr = 0;
  word endAddr = 0x3FFF;

  progress_begin("Writing Test", "Starting...");
  for (int currBank = 0; currBank < testBanks; currBank++) 
  {
      // Blink led
      LED_BLUE_BLINK;  // Blue for writing
      progress_update(currBank, testBanks);
      char bankStatus[32];
      sprintf(bankStatus, "Writing test bank %d/%d", currBank + 1, testBanks);
      progress_set_status(bankStatus);

      // Set ROM bank
      writeByte_GB_WithPin(0x2100, currBank & 0xFF, WRITE_PIN_WR);
      writeByte_GB_WithPin(0x3000, (currBank >> 8) & 0x01, WRITE_PIN_WR);
      
      // Add delay after bank switch for stability
      delay(1);

      if (currBank > 0) 
      {
        currAddr = 0x4000;
        endAddr = 0x7FFF;
      }
      else 
      {
        currAddr = 0;
        endAddr = 0x3FFF;
      }
      //else
      {
        // 0x2A8000 fix        
      }

      while (currAddr <= endAddr)
      {
        for (int currByte = 0; currByte < 512; currByte++) 
        {
          // Write command sequence
          // Get chip info if available
          const flash_chip_info_t* curr_chip_info = NULL;
          if (flash_mfg_id != 0 && flash_dev_id != 0) {
            curr_chip_info = gb_flash_get_chip_info(flash_mfg_id, flash_dev_id);
          }
          
          if (curr_chip_info && curr_chip_info->commands) {
            // Use chip-specific write sequence
            for (int i = 0; i < curr_chip_info->commands->write_sequence_count; i++) {
              writeByteCompensatedWithPin(curr_chip_info->commands->write_sequence[i].address,
                                        curr_chip_info->commands->write_sequence[i].data,
                                        curr_chip_info->write_pin);
            }
          } else {
            // Default AAA/555 commands
            writeByteCompensatedWithPin(0xAAA, 0xaa, WRITE_PIN_WR);
            writeByteCompensatedWithPin(0x555, 0x55, WRITE_PIN_WR);
            writeByteCompensatedWithPin(0xAAA, 0xa0, WRITE_PIN_WR);
          }
          byte tb = currByte & 0xFF;
          // Write current byte
          writeByteCompensatedWithPin(currAddr + currByte, tb, WRITE_PIN_AUDIO);

          delay_GB();
          // Set data pins to input
          dataIn_GB();

          delay_GB();
          // Setting CS(PH3) and OE/RD(PH6) LOW
          //PORTH &= ~((1 << 3) | (1 << 6));
          gpio_bit_reset(CTRL,CS);

          //delay_GB();
          gpio_bit_reset(CTRL,RD);
          //delay_GB();

          // Busy check
          short i = 0;

          //for(i = 0;i<40;i++){delay_GB();}          
          while (((GPIO_ISTAT(DATA) >> 8) & 0x80) != (tb & 0x80)) 
          {
            i++;
            if (i > 2000) 
            {
              if (currAddr >= 0x4000) 
              { 
                // This happens when trying to flash an MBC5 as if it was an MBC3. Retry to flash as MBC5, starting from last successfull byte.
                currByte--;
                currAddr += 0x4000;
                endAddr = 0x7FFF;
                break;
              } 
              else 
              { 
                return;
              }
            }
          }

          // Switch CS(PH3) and OE/RD(PH6) to HIGH
          gpio_bit_set(CTRL,RD);
          gpio_bit_set(CTRL,CS);
          
          // Waste a few CPU cycles to remove write errors
          delay_GB();
          delay_GB();
          delay_GB();

          // Set data pins to output
          dataOut_GB();
        }
        currAddr += 512;
      }
  }
  progress_update(testBanks, testBanks);
  progress_complete("Test write complete");

  // Set data pins to input again
  dataIn_GB();
  uint32_t wErrors = 0;
  // Verify flashrom
  word romAddress = 0;
  // Read number of banks and switch banks
  progress_begin("Verifying Test", "Starting...");
  for (word bank = 1; bank < testBanks; bank++) 
  {
      // Switch data pins to output
      dataOut_GB();
      if (romType >= 5) { // MBC2 and above
        writeByte_GB_WithPin(0x2100, bank, WRITE_PIN_WR); // Set ROM bank
      }
      else { // MBC1
        writeByte_GB_WithPin(0x6000, 0, WRITE_PIN_WR); // Set ROM Mode
        writeByte_GB_WithPin(0x4000, bank >> 5, WRITE_PIN_WR); // Set bits 5 & 6 (01100000) of ROM bank
        writeByte_GB_WithPin(0x2000, bank & 0x1F, WRITE_PIN_WR); // Set bits 0 & 4 (00011111) of ROM bank
      }

      // Switch data pins to intput
      dataIn_GB();
      if (bank > 1) {
        romAddress = 0x4000;
      }
      // Blink led
      LED_BLUE_BLINK;  // Blue for writing
      progress_update(bank - 1, testBanks - 1);
      char bankStatus[32];
      sprintf(bankStatus, "Verifying bank %d/%d", bank, testBanks);
      progress_set_status(bankStatus);

      // Read up to 7FFF per bank
      while (romAddress <= 0x7FFF) 
      {
        // Compare
        for (int i = 0; i < 512; i++) {
          byte tb = i & 0xFF;
          if (readByte_GB(romAddress + i) != tb) {
            wErrors++;
          }
        }
        romAddress += 512;
      }
  }
  progress_update(testBanks - 1, testBanks - 1);
  progress_complete("Verification complete");

  if (wErrors == 0) {
      OledShowString(0,6,"ROM Test OK!",8);
  }
  else {
      char msgbuf[64] = {0};
      sprintf(msgbuf,"Error:%d bytes",wErrors);
      OledShowString(0,6,msgbuf,8);
      print_Error("Did not verify...", false);
  }
}



void TestMemGB(boolean bFast)
{
  //
  setup_GB();
  identifyCFI_GB();
  if(bFast){
    TestSramGB(8,0xA100);
    testCFI_GB(8);
  }else{
    TestSramGB(8,0xBFFF);
    testCFI_GB(512);
  }
  OledShowString(0,7,"Press OK Button...",8);
  WaitOKBtn();
  ResetSystem();
}


// GB Flash items
static const char GBFlashItem1[] = "Flash Cart";
static const char GBFlashItem2[] = "Flash Cart and Save";
static const char GBFlashItem3[] = "29F Cart (MBC3)";
static const char GBFlashItem4[] = "29F Cart (MBC5)";
static const char GBFlashItem5[] = "29F Cart (CAM)";
static const char GBFlashItem6[] = "Auto Detect Flash";
static const char GBFlashItem7[] = "Detect Flash (AUDIO)";
static const char GBFlashItem8[] = "Reset";
static const char* const menuOptionsGBFlash[] = {GBFlashItem1, GBFlashItem2, GBFlashItem3, GBFlashItem4, GBFlashItem5, GBFlashItem6, GBFlashItem7, GBFlashItem8};


uint8_t gbFlashMenu()
{
  //
  uint8_t bret = 0;

  unsigned char gbFlash = questionBox_OLED("Select type:", menuOptionsGBFlash, 8, 1, 1, 1);
  
  char flash_msg[64];
  sprintf(flash_msg, "GB Flash Menu: Selected option %d\r\n", gbFlash);
  soft_uart_send_string(flash_msg);
  
  OledClear();
  // wait for user choice to come back from the question box menu
  switch (gbFlash)
  {
    case 0:
      //cancel btn clicked
      bret = 1;
      break;
 

    case 1:
      // Flash CFI
      // Launch filebrowser
      fileBrowser("/","Select file:");
      OledClear();
      identifyCFI_GB();
      soft_uart_send_string("GB Flash: Calling writeCFI_GB()\r\n");
      bool result = writeCFI_GB();
      soft_uart_send_string("GB Flash: writeCFI_GB() returned\r\n");
      if (!result) {
        soft_uart_send_string("GB Flash: writeCFI_GB() returned false - showing timeout error\r\n");
        OledClear();
        OledShowString(0,0,"Flashing failed\nTime out!",8);
      } else {
        soft_uart_send_string("GB Flash: writeCFI_GB() returned true - success!\r\n");
        // Show success message and wait for user
        OledShowString(0,7,"Press OK to continue",8);
        WaitOKBtn();
      }
      break;

    case 2:
      // Flash CFI and Save
      soft_uart_send_string("GB Flash: Launching file browser\r\n");
      fileBrowser("/","Select file:");
      soft_uart_send_string("GB Flash: File browser returned\r\n");
      OledClear();
      identifyCFI_GB();
      if (!writeCFI_GB()) {
        //
        print_Error("Flashing failed!\n Time out!",true);
      }
      getCartInfo_GB();
      // Does cartridge have SRAM
      if (lastByte > 0) 
      {
        //
        OledClear();
        OledShowString(0,0,"Save Sram Data:",8);
        //Get the save file name
        char * cpos = strrchr(filePath,'/');
        if(cpos){cpos++;strcpy(fileName,cpos);}
        else strcpy(fileName,filePath);
        //Remove file ext name
        int pos = -1;
        while (fileName[++pos] != '\0') {
          if (fileName[pos] == '.') {
            fileName[pos] = '\0';
            break;
          }
        }


        sprintf(filePath, "/GB/SAVE/%s/", fileName);
        bool saveFound = false;
        FILINFO tfinfo;
        if (f_stat(filePath,&tfinfo) == FR_OK) 
        {
          foldern = load_dword();
          for (int i = foldern; i >= 0; i--) 
          {
            sprintf(filePath, "/GB/SAVE/%s/%d/%s.SAV", fileName, i, fileName);
            if (f_stat(filePath,&tfinfo) == FR_OK) 
            {
              //
              char tmsg[64] = {0};
              sprintf(tmsg,"Save number %d found.",i);
              OledShowString(0,6,tmsg,8);
              saveFound = true;

              writeSRAM_GB();

              unsigned long wrErrors = verifySRAM_GB();
              if (wrErrors == 0) 
              {
                OledShowString(0,6,"Verified OK",8);
              }
              else 
              {
                sprintf(tmsg,"Error: %d bytes.",wrErrors);
                OledShowString(0,6,tmsg,8);
                print_Error("Did not verify...", false);
              }
              break;
            }
          }
        }
        
        if (!saveFound) 
        {
          OledShowString(0,1,"Error: No save found.",8);
        }
      }
      else 
      {
        print_Error("Cart has no Sram", false);
      }
      break;

   case 3:
      //Flash MBC3
      writeFlash29F_GB(3, 1);
      // Reset
      break;

   case 4:
      //Flash MBC5
      writeFlash29F_GB(5, 1);
      break;
   case 5:
      //Flash GB Camera
      //MBC3
      writeFlash29F_GB(3, 1);
      OledShowString(0,7,"Press OK Button...",8);
      WaitOKBtn();

      OledClear();
      OledShowString(0,0,"Please change the",8);
      OledShowString(0,1,"switch on the cart",8);
      OledShowString(0,2,"to B2 (Bank 2)",8);
      OledShowString(0,3,"if you want to flash",8);
      OledShowString(0,4,"a second game",8);

      OledShowString(0,7,"Press OK Button...",8);
      WaitOKBtn();

      // Flash second bank without erase
      // Change working dir to root
      //MBC3
      writeFlash29F_GB(3, 0);
      break;

      /*
    case 6:
      // Flash GB Smart
      setup_GBSmart();
      mode = mode_GB_GBSmart;
      break;*/

    case 6:
      // Auto Detect Flash
      soft_uart_send_string("GB Flash: Auto-detecting flash chip\r\n");
      OledClear();
      OledShowString(0, 0, "Detecting Flash...", 8);
      
      // Initialize flash interface
      gb_flash_init();
      
      // Try to detect the flash chip using enhanced detection
      const flash_chip_info_t* chip_info = gb_flash_detect_and_match(WRITE_PIN_WR);
      if (chip_info) {
        char debug_msg[64];
        sprintf(debug_msg, "GB Flash: Found chip: %s\r\n", chip_info->name);
        soft_uart_send_string(debug_msg);
        
        // Check if chip requires special handling
        if (chip_info->type == FLASH_CFI) {
          soft_uart_send_string("GB Flash: CFI chip detected - using CFI mode\r\n");
          char info_msg[64];
          OledClear();
          sprintf(info_msg, "%s", chip_info->name);
          OledShowString(0, 0, info_msg, 8);
          OledShowString(0, 1, "Using CFI mode", 8);
          OledShowString(0, 3, "Select ROM file", 8);
          delay(2000);
          
          // Launch file browser for CFI mode
          fileBrowser("/", "Select file:");
          if (strlen(filePath) > 0) {
            OledClear();
            identifyCFI_GB();
            if (!writeCFI_GB()) {
              OledClear();
              OledShowString(0,0,"Flashing failed\nTime out!",8);
              OledShowString(0,7,"Press OK",8);
              WaitOKBtn();
            } else {
              // Success - wait for user
              soft_uart_send_string("GB Flash: Auto-detect flash completed successfully\r\n");
              OledShowString(0,7,"Press OK to continue",8);
              WaitOKBtn();
            }
          }
          break;  // Exit the flash menu
        }
        
        if (chip_info) {
          soft_uart_send_string("GB Flash: Chip found in database\r\n");
          char info_msg[64];
          OledClear();
          sprintf(info_msg, "Found: %s", chip_info->name);
          OledShowString(0, 0, info_msg, 8);
          sprintf(info_msg, "ID: %02X:%02X", chip_info->manufacturer_id, chip_info->device_id);
          OledShowString(0, 1, info_msg, 8);
          sprintf(info_msg, "Size: %dKB", chip_info->size / 1024);
          OledShowString(0, 2, info_msg, 8);
          OledShowString(0, 3, "Auto-flashing...", 8);
          
          soft_uart_send_string("Auto-detect: Proceeding to flash automatically\r\n");
          // Short delay to let user see the info
          delay(1500);
          
          {
            // Let user select file
            soft_uart_send_string("GB Flash: Launching file browser\r\n");
            fileBrowser("/", "Select ROM file:");
            
            if (strlen(filePath) > 0) {
              // Erase chip using progress system
              progress_begin("Flash 00:C3", "Erasing...");
              soft_uart_send_string("GB Flash: Erasing chip\r\n");
              if (!gb_flash_erase_chip(chip_info)) {
                soft_uart_send_string("GB Flash: Erase failed!\r\n");
                print_Error("Erase failed!", true);
              }
              progress_complete("Erase complete");
              soft_uart_send_string("GB Flash: Erase complete\r\n");
              
              // Check if erase worked by reading a few bytes
              uint8_t erase_check[4];
              for (int i = 0; i < 4; i++) {
                erase_check[i] = readByte_GB(i);
              }
              char check_msg[64];
              sprintf(check_msg, "GB Flash: After erase, first 4 bytes: %02X %02X %02X %02X\r\n", 
                      erase_check[0], erase_check[1], erase_check[2], erase_check[3]);
              soft_uart_send_string(check_msg);
              
              // Open file and write
              FIL tf;
              UINT rdt;
              if (f_open(&tf, filePath, FA_READ) == FR_OK) {
                uint32_t file_size = f_size(&tf);
                uint32_t bytes_written = 0;
                uint8_t buffer[512];
                uint8_t current_bank = 0;
                
                // Calculate actual bytes to write (minimum of file size and chip size)
                uint32_t total_to_write = (file_size < chip_info->size) ? file_size : chip_info->size;
                
                char size_msg[64];
                sprintf(size_msg, "GB Flash: File=%luKB, Chip=%luKB, Writing=%luKB\r\n", 
                        file_size/1024, chip_info->size/1024, total_to_write/1024);
                soft_uart_send_string(size_msg);
                
                // Write operation using progress system
                progress_begin("Writing Flash", "Bank 0");
                
                // Notify about clone chip timing
                if (chip_info->manufacturer_id == 0xF3 || chip_info->manufacturer_id == 0x00) {
                  soft_uart_send_string("GB Flash: Clone chip detected, using adjusted timing\r\n");
                }
                
                // Set data pins to output
                dataOut_GB();
                
                // Initialize bank registers
                writeByte_GB(0x3000, 0);  // ROM bank hi 0
                writeByte_GB(0x2000, 0);  // ROM bank low 0 
                writeByte_GB(0x4000, 0);  // Additional bank register
                delay(100);
                
                // Write bank 0 first (0x0000-0x3FFF)
                soft_uart_send_string("GB Flash: Writing bank 0\r\n");
                current_bank = 0;
                writeByte_GB(0x2000, 0);  // Ensure bank 0 is selected
                
                uint32_t bank_0_size = (total_to_write < 0x4000) ? total_to_write : 0x4000;
                for (uint32_t addr = 0; addr < bank_0_size; addr += 512) {
                  UINT bytes_read;
                  f_read(&tf, buffer, 512, &bytes_read);
                  if (bytes_read == 0) break;
                  
                  for (uint32_t i = 0; i < bytes_read && (addr + i) < bank_0_size; i++) {
                    if (!gb_flash_write_byte(chip_info, addr + i, buffer[i])) {
                      f_close(&tf);
                      char err_msg[64];
                      sprintf(err_msg, "Write failed at %04X", addr + i);
                      print_Error(err_msg, true);
                    }
                  }
                  
                  bytes_written = addr + bytes_read;
                  progress_update(bytes_written, total_to_write);
                  LED_RED_BLINK;  // Red for verifying  // Blue for writing
                }
                
                // Write remaining banks (1+)
                while (bytes_written < total_to_write) {
                  uint8_t bank = bytes_written / 0x4000;
                  
                  // Switch to new bank
                  if (bank != current_bank) {
                    // Don't print every bank - it floods the serial
                    if ((bank % 16) == 0) {  // Only print every 16th bank
                      char bank_msg[64];
                      sprintf(bank_msg, "GB Flash: Writing bank %d\r\n", bank);
                      soft_uart_send_string(bank_msg);
                    }
                    
                    // Ensure data pins are output for bank switching
                    dataOut_GB();
                    writeByte_GB(0x2000, bank);  // Bank low bits
                    writeByte_GB(0x4000, 0x0);   // 0x2A8000 fix (bank high bits)
                    current_bank = bank;
                    delay(10);  // Give time for bank switch
                    
                    // Update status to show current bank
                    char status_msg[32];
                    sprintf(status_msg, "Bank %d", bank);
                    progress_set_status(status_msg);
                  }
                  
                  // Calculate how much to write in this bank
                  uint32_t bank_start = bank * 0x4000;
                  uint32_t bank_offset = bytes_written - bank_start;
                  uint32_t bytes_in_bank = 0x4000 - bank_offset;
                  if (bytes_written + bytes_in_bank > total_to_write) {
                    bytes_in_bank = total_to_write - bytes_written;
                  }
                  
                  // Read and write data for this bank
                  UINT bytes_read;
                  f_read(&tf, buffer, (bytes_in_bank < 512) ? bytes_in_bank : 512, &bytes_read);
                  if (bytes_read == 0) break;
                  
                  for (uint32_t i = 0; i < bytes_read; i++) {
                    uint16_t write_addr = 0x4000 + bank_offset + i;
                    if (!gb_flash_write_byte(chip_info, write_addr, buffer[i])) {
                      f_close(&tf);
                      char err_msg[64];
                      sprintf(err_msg, "Write failed at bank %d addr %04X", bank, write_addr);
                      print_Error(err_msg, true);
                    }
                  }
                  
                  bytes_written += bytes_read;
                  progress_update(bytes_written, total_to_write);
                  LED_RED_BLINK;  // Red for verifying  // Blue for writing
                }
                
                // Switch back to bank 0
                writeByte_GB(0x2000, 0);
                dataIn_GB();
                
                f_close(&tf);
                
                // Complete write operation
                progress_complete("Write complete");
                
                // Brief delay to show completion message
                delay(1000);
                
                // Verify the flash using progress system
                progress_begin("Verifying Flash", "Bank 0");
                soft_uart_send_string("GB Flash: Starting verification\r\n");
                if (f_open(&tf, filePath, FA_READ) == FR_OK) {
                  uint32_t verify_errors = 0;
                  uint32_t bytes_verified = 0;
                  current_bank = 0;
                  
                  // Calculate actual bytes to verify (minimum of file size and chip size)
                  uint32_t total_to_verify = (file_size < chip_info->size) ? file_size : chip_info->size;
                  
                  char verify_msg[64];
                  sprintf(verify_msg, "GB Flash: Verifying %luKB\r\n", total_to_verify / 1024);
                  soft_uart_send_string(verify_msg);
                  
                  // Set data pins to output for bank switching
                  dataOut_GB();
                  writeByte_GB(0x2000, 0);  // Start at bank 0
                  dataIn_GB();
                  while (bytes_verified < file_size && bytes_verified < chip_info->size) {
                    UINT bytes_read;
                    f_read(&tf, buffer, sizeof(buffer), &bytes_read);
                    if (bytes_read == 0) break;
                    
                    // Read back from flash and compare
                    for (uint32_t i = 0; i < bytes_read; i++) {
                      uint32_t target_addr = bytes_verified + i;
                      uint8_t bank = target_addr / 0x4000;  // 16KB banks
                      uint16_t bank_addr = target_addr & 0x3FFF;  // Address within bank
                      
                      // Switch bank if needed
                      if (bank != current_bank && bank > 0) {
                        char status_msg[32];
                        sprintf(status_msg, "Bank %d", bank);
                        progress_set_status(status_msg);
                        dataOut_GB();
                        writeByte_GB(0x2000, bank);  // MBC bank switch
                        current_bank = bank;
                        dataIn_GB();
                        delay(1);
                      }
                      
                      // For bank 0, use direct address
                      // For other banks, map to 0x4000-0x7FFF range
                      uint16_t read_addr = (bank == 0) ? bank_addr : (0x4000 + bank_addr);
                      uint8_t flash_byte = readByte_GB(read_addr);
                      
                      if (flash_byte != buffer[i]) {
                        verify_errors++;
                        // Don't print errors during verification - it messes up the display
                      }
                    }
                    
                    bytes_verified += bytes_read;
                    // Update progress BEFORE any error printing
                    progress_update(bytes_verified, total_to_verify);
                    LED_RED_BLINK;  // Red for verifying
                  }
                  
                  f_close(&tf);
                  
                  if (verify_errors == 0) {
                    progress_complete("Flash verified OK!");
                    soft_uart_send_string("GB Flash: Verification successful\r\n");
                    OledShowString(0, 7, "Press OK...", 8);
                  } else {
                    char err_msg[64];
                    sprintf(err_msg, "Failed: %lu errors", verify_errors);
                    progress_complete(err_msg);
                    sprintf(err_msg, "GB Flash: Verification failed with %lu errors\r\n", verify_errors);
                    soft_uart_send_string(err_msg);
                    OledShowString(0, 7, "Press OK...", 8);
                  }
                } else {
                  progress_complete("Cannot verify");
                  soft_uart_send_string("GB Flash: Could not verify (file reopen failed)\r\n");
                  OledShowString(0, 7, "Press OK...", 8);
                }
              } else {
                print_Error("File open failed!", true);
              }
            }
          }
        } else {
          soft_uart_send_string("GB Flash: No flash chip detected\r\n");
          OledClear();
          OledShowString(0, 0, "No flash chip", 8);
          OledShowString(0, 1, "detected", 8);
          OledShowString(0, 4, "Press OK to exit", 8);
          WaitOKBtn();
        }
      } else {
        OledShowString(0, 2, "No flash detected!", 8);
        OledShowString(0, 3, "Check connections", 8);
      }
      exit_flash:
      break;

    case 7:
      // Auto Detect Flash with AUDIO pin
      soft_uart_send_string("GB Flash: Auto-detecting flash chip with AUDIO pin\r\n");
      OledClear();
      OledShowString(0, 0, "Detecting Flash...", 8);
      OledShowString(0, 1, "Using AUDIO pin", 8);
      
      // Initialize flash interface
      gb_flash_init();
      
      // Try to detect the flash chip using AUDIO pin
      const flash_chip_info_t* audio_chip_info = gb_flash_detect_and_match(WRITE_PIN_AUDIO);
      if (audio_chip_info) {
        char debug_msg[64];
        sprintf(debug_msg, "GB Flash: Found chip on AUDIO: %s\r\n", audio_chip_info->name);
        soft_uart_send_string(debug_msg);
        
        // Check if chip requires CFI mode
        if (audio_chip_info->type == FLASH_CFI) {
          soft_uart_send_string("GB Flash: CFI chip detected on AUDIO pin - using CFI mode\r\n");
          OledClear();
          OledShowString(0, 0, audio_chip_info->name, 8);
          OledShowString(0, 1, "Using CFI mode", 8);
          OledShowString(0, 3, "Select ROM file", 8);
          delay(2000);
          
          // Launch file browser for CFI mode
          fileBrowser("/", "Select file:");
          if (strlen(filePath) > 0) {
            OledClear();
            identifyCFI_GB();
            if (!writeCFI_GB()) {
              OledClear();
              OledShowString(0,0,"Flashing failed\nTime out!",8);
              OledShowString(0,7,"Press OK",8);
              WaitOKBtn();
            } else {
              // Success - wait for user
              soft_uart_send_string("GB Flash: Auto-detect flash completed successfully\r\n");
              OledShowString(0,7,"Press OK to continue",8);
              WaitOKBtn();
            }
          }
          break;  // Exit the flash menu
        }
        
        // Display chip info
        if (audio_chip_info) {
          soft_uart_send_string("GB Flash: AUDIO pin chip detected\r\n");
          
          char info_msg[64];
          OledClear();
          sprintf(info_msg, "Found: %s", audio_chip_info->name);
          OledShowString(0, 0, info_msg, 8);
          sprintf(info_msg, "ID: %02X:%02X", audio_chip_info->manufacturer_id, audio_chip_info->device_id);
          OledShowString(0, 1, info_msg, 8);
          sprintf(info_msg, "Size: %dKB", audio_chip_info->size / 1024);
          OledShowString(0, 2, info_msg, 8);
          sprintf(info_msg, "Pin: AUDIO");
          OledShowString(0, 3, info_msg, 8);
          OledShowString(0, 4, "Press OK to flash", 8);
          OledShowString(0, 5, "or Cancel to exit", 8);
          
          soft_uart_send_string("Waiting for button press...\r\n");
          // Wait for user input
          while(1) {
            uint8_t btn = checkButton();
            if (btn == BTNOK) {
              soft_uart_send_string("OK pressed, proceeding to flash\r\n");
              break;
            } else if (btn == BTNCANCEL) {
              soft_uart_send_string("Cancel pressed, exiting\r\n");
              goto exit_audio_flash;
            }
          }
          
          // Verify chip uses AUDIO pin
          if (!audio_chip_info || audio_chip_info->write_pin != WRITE_PIN_AUDIO) {
            OledClear();
            OledShowString(0, 0, "Error:", 8);
            OledShowString(0, 1, "No AUDIO flash", 8);
            OledShowString(0, 2, "detected", 8);
            soft_uart_send_string("GB Flash: No AUDIO flash chip detected\r\n");
            delay(2000);
            goto exit_audio_flash;
          }
          
          // Let user select file
          soft_uart_send_string("GB Flash: Launching file browser for AUDIO flash\r\n");
          fileBrowser("/", "Select ROM file:");
          
          if (strlen(filePath) > 0) {
            // Erase chip using AUDIO pin
            progress_begin("Flash (AUDIO)", "Erasing...");
            soft_uart_send_string("GB Flash: Erasing chip with AUDIO pin\r\n");
            if (!gb_flash_erase_chip(audio_chip_info)) {
              soft_uart_send_string("GB Flash: Erase failed!\r\n");
              print_Error("Erase failed!", true);
            }
            progress_complete("Erase complete");
            soft_uart_send_string("GB Flash: Erase complete\r\n");
            
            // Check if erase worked by reading a few bytes
            uint8_t erase_check[4];
            for (int i = 0; i < 4; i++) {
              erase_check[i] = readByte_GB(i);
            }
            char check_msg[64];
            sprintf(check_msg, "GB Flash: After AUDIO erase, first 4 bytes: %02X %02X %02X %02X\r\n", 
                    erase_check[0], erase_check[1], erase_check[2], erase_check[3]);
            soft_uart_send_string(check_msg);
            
            // If not erased properly, warn user
            if (erase_check[0] != 0xFF || erase_check[1] != 0xFF) {
              soft_uart_send_string("GB Flash: WARNING - Chip not erased! Trying different algorithm\r\n");
              OledClear();
              OledShowString(0, 0, "Erase failed!", 8);
              OledShowString(0, 1, "Chip not blank", 8);
              OledShowString(0, 2, "Try CFI mode", 8);
              delay(3000);
              goto exit_audio_flash;
            }
            
            // The rest of the flash code is identical to option 6
            // Just uses audio_chip_info which has WRITE_PIN_AUDIO set
            
            // Open file and write
            FIL tf;
            UINT rdt;
            if (f_open(&tf, filePath, FA_READ) == FR_OK) {
              uint32_t file_size = f_size(&tf);
              uint32_t bytes_written = 0;
              uint8_t buffer[512];
              uint8_t current_bank = 0;
              
              uint32_t total_to_write = (file_size < audio_chip_info->size) ? file_size : audio_chip_info->size;
              
              char size_msg[64];
              sprintf(size_msg, "GB Flash: File=%luKB, Chip=%luKB, Writing=%luKB (AUDIO pin)\r\n", 
                      file_size/1024, audio_chip_info->size/1024, total_to_write/1024);
              soft_uart_send_string(size_msg);
              
              progress_begin("Writing (AUDIO)", "Bank 0");
              
              dataOut_GB();
              
              writeByte_GB(0x3000, 0);
              writeByte_GB(0x2000, 0);
              writeByte_GB(0x4000, 0);
              delay(100);
              
              // Write bank 0
              soft_uart_send_string("GB Flash: Writing bank 0 with AUDIO pin\r\n");
              current_bank = 0;
              writeByte_GB(0x2000, 0);
              
              uint32_t bank_0_size = (total_to_write < 0x4000) ? total_to_write : 0x4000;
              for (uint32_t addr = 0; addr < bank_0_size; addr += 512) {
                UINT bytes_read;
                f_read(&tf, buffer, 512, &bytes_read);
                if (bytes_read == 0) break;
                
                for (uint32_t i = 0; i < bytes_read && (addr + i) < bank_0_size; i++) {
                  if (!gb_flash_write_byte(audio_chip_info, addr + i, buffer[i])) {
                    f_close(&tf);
                    char err_msg[64];
                    sprintf(err_msg, "Write failed at %04X", addr + i);
                    print_Error(err_msg, true);
                  }
                }
                
                bytes_written = addr + bytes_read;
                progress_update(bytes_written, total_to_write);
                LED_BLUE_BLINK;  // Blue for writing
              }
              
              // Write remaining banks
              while (bytes_written < total_to_write) {
                uint8_t bank = bytes_written / 0x4000;
                
                if (bank != current_bank) {
                  if ((bank % 16) == 0) {
                    char bank_msg[64];
                    sprintf(bank_msg, "GB Flash: Writing bank %d (AUDIO)\r\n", bank);
                    soft_uart_send_string(bank_msg);
                  }
                  
                  dataOut_GB();
                  writeByte_GB(0x2000, bank);
                  writeByte_GB(0x4000, 0x0);
                  current_bank = bank;
                  delay(10);
                  
                  char status_msg[32];
                  sprintf(status_msg, "Bank %d", bank);
                  progress_set_status(status_msg);
                }
                
                uint32_t bank_start = bank * 0x4000;
                uint32_t bank_offset = bytes_written - bank_start;
                uint32_t bytes_in_bank = 0x4000 - bank_offset;
                if (bytes_written + bytes_in_bank > total_to_write) {
                  bytes_in_bank = total_to_write - bytes_written;
                }
                
                UINT bytes_read;
                f_read(&tf, buffer, (bytes_in_bank < 512) ? bytes_in_bank : 512, &bytes_read);
                if (bytes_read == 0) break;
                
                for (uint32_t i = 0; i < bytes_read; i++) {
                  uint16_t write_addr = 0x4000 + bank_offset + i;
                  if (!gb_flash_write_byte(audio_chip_info, write_addr, buffer[i])) {
                    f_close(&tf);
                    char err_msg[64];
                    sprintf(err_msg, "Write failed at bank %d addr %04X", bank, write_addr);
                    print_Error(err_msg, true);
                  }
                }
                
                bytes_written += bytes_read;
                progress_update(bytes_written, total_to_write);
                LED_BLUE_BLINK;  // Blue for writing
              }
              
              writeByte_GB(0x2000, 0);
              dataIn_GB();
              
              f_close(&tf);
              
              progress_complete("Write complete");
              
              soft_uart_send_string("GB Flash: Write complete, starting verification\r\n");
              delay(1000);
              
              // Verify
              progress_begin("Verifying (AUDIO)", "Bank 0");
              if (f_open(&tf, filePath, FA_READ) == FR_OK) {
                uint32_t verify_errors = 0;
                uint32_t bytes_verified = 0;
                current_bank = 0;
                
                uint32_t total_to_verify = (file_size < audio_chip_info->size) ? file_size : audio_chip_info->size;
                
                writeByte_GB(0x2000, 0);
                dataIn_GB();
                
                while (bytes_verified < total_to_verify && verify_errors < 100) {
                  uint8_t bank = bytes_verified / 0x4000;
                  
                  if (bank != current_bank) {
                    writeByte_GB(0x2000, bank);
                    current_bank = bank;
                    
                    char status_msg[32];
                    sprintf(status_msg, "Bank %d", bank);
                    progress_set_status(status_msg);
                  }
                  
                  uint32_t bank_start = bank * 0x4000;
                  uint32_t bank_offset = bytes_verified - bank_start;
                  uint16_t read_addr = (bank == 0 && bank_offset < 0x4000) ? bank_offset : (0x4000 + bank_offset);
                  
                  UINT bytes_read;
                  f_read(&tf, buffer, 512, &bytes_read);
                  if (bytes_read == 0) break;
                  
                  for (uint32_t i = 0; i < bytes_read && bytes_verified < total_to_verify; i++) {
                    uint8_t cart_byte = readByte_GB(read_addr + i);
                    if (cart_byte != buffer[i]) {
                      verify_errors++;
                      if (verify_errors <= 10) {
                        char err_msg[80];
                        sprintf(err_msg, "Verify error at %06lX: expected %02X, got %02X\r\n", 
                                bytes_verified + i, buffer[i], cart_byte);
                        soft_uart_send_string(err_msg);
                      }
                    }
                  }
                  
                  bytes_verified += bytes_read;
                  progress_update(bytes_verified, total_to_verify);
                  LED_RED_BLINK;  // Red for verifying
                }
                
                f_close(&tf);
                
                if (verify_errors > 0) {
                  char err_msg[64];
                  sprintf(err_msg, "Verify failed: %lu errors", verify_errors);
                  progress_complete(err_msg);
                  soft_uart_send_string("GB Flash: Verification FAILED with AUDIO pin\r\n");
                  delay(3000);
                } else {
                  progress_complete("Verify OK!");
                  soft_uart_send_string("GB Flash: Verification successful with AUDIO pin!\r\n");
                  delay(2000);
                }
              }
            }
          }
          
        } else {
          soft_uart_send_string("GB Flash: Chip info display issue\r\n");
          OledClear();
          OledShowString(0, 0, "Flash detected", 8);
          OledShowString(0, 1, "but display error", 8);
          OledShowString(0, 3, "Uses AUDIO pin", 8);
        }
      } else {
        soft_uart_send_string("GB Flash: No flash detected with AUDIO pin\r\n");
        OledClear();
        OledShowString(0, 0, "Flash not detected!", 8);
        OledShowString(0, 1, "with AUDIO pin", 8);
        OledShowString(0, 3, "Check connections", 8);
      }
      exit_audio_flash:
      break;

    case 8:
      ResetSystem();
      break;
  }

  if(bret == 0)
  {
    // Reset
    OledShowString(0,7,"Press OK Button...",8);
    WaitOKBtn();
    ResetSystem();
  }
  return bret;
}


void gbFlashScreen()
{
  while(1)
  {
    //
    setup_GB();
    uint8_t b = gbFlashMenu();
    if(b>0)break;
  }
}

// GB menu items
static const char GBMenuItem1[] = "Flash GBC Cart";
static const char GBMenuItem2[] = "Read Rom";
static const char GBMenuItem3[] = "Read Save";
static const char GBMenuItem4[] = "Write Save";
static const char GBMenuItem5[] = "NPower GB Memory";
static const char GBMenuItem6[] = "Reset";
static const char* const menuOptionsGB[] = {GBMenuItem1, GBMenuItem2, GBMenuItem3, GBMenuItem4, GBMenuItem5, GBMenuItem6};

uint8_t gbMenu() 
{
  //
  uint8_t bret = 0;
  
  // create menu with title and 3 options to choose from
  unsigned char gbMenu = questionBox_OLED("GB Cart Reader", menuOptionsGB, 6, 1, 1, 1);

  // wait for user choice to come back from the question box menu
  char menu_msg[64];
  sprintf(menu_msg, "GB Menu: Selected option %d\r\n", gbMenu);
  soft_uart_send_string(menu_msg);
  
  switch (gbMenu)
  {
    case 0:
      //cancel btn clicked
      bret = 1;
      break;
    case 1:
      gbFlashScreen();
      break;
    case 2:
      soft_uart_send_string("GB Menu: Read ROM selected\r\n");
      OledClear();
      // Change working dir to root
      //f_chdir("/");
      soft_uart_send_string("GB Menu: Calling readROM_GB()\r\n");
      readROM_GB();
      soft_uart_send_string("GB Menu: Calling compare_checksum_GB()\r\n");
      compare_checksum_GB();
      soft_uart_send_string("GB Menu: Read ROM complete\r\n");
      break;

    case 3:
      OledClear();
      // Does cartridge have SRAM
      if (lastByte > 0) {
      // Change working dir to root
        f_chdir("/");
        readSRAM_GB();
      }
      else {
        print_Error("Cart has no Sram", false);
      }
      break;

    case 4:
      OledClear();
      // Does cartridge have SRAM
      if (lastByte > 0) 
      {
        // Change working dir to root
        f_chdir("/");
        filePath[0] = '\0';
        fileBrowser("/","Select sav file");
        writeSRAM_GB();
        OledClear();
        unsigned long wrErrors;
        wrErrors = verifySRAM_GB();
        if (wrErrors == 0) 
        {
          OledShowString(0,2,"Verified OK",8);
        }
        else 
        {
          char tbufp[30] = {0};
          sprintf(tbufp,"Error: %d bytes.",wrErrors);
          OledShowString(0,1,tbufp,8);
          print_Error("did not verify.", false);
        }
      }
      else {
        print_Error("Cart has no Sram", false);
      }
      break;

    case 5:
      // Flash GB Memory
      gbmScreen();
      break;
    case 6:
      // Return to main menu
      bret = 1;
      break;
  }

  //OledClear();
  if(bret == 0)
  {
    OledShowString(0,7,"Press OK Button...",8);
    WaitOKBtn();
  }
  return bret;
}


void gbScreen()
{
  soft_uart_send_string("GB Screen: Starting\r\n");
  while(1)
  {
    //
    soft_uart_send_string("GB Screen: Calling setup_GB()\r\n");
    setup_GB();
    soft_uart_send_string("GB Screen: Calling gbMenu()\r\n");
    uint8_t b = gbMenu();
    if(b>0) {
      soft_uart_send_string("GB Screen: Exiting\r\n");
      break;
    }
  }
}




// Helper functions for ROM operations
uint32_t gb_get_rom_size_bytes(uint8_t romSize) {
  if (romSize < 8) {
    return (uint32_t)(1 << (romSize + 1)) * 16384;
  } else {
    // Special cases for larger sizes
    switch(romSize) {
      case 0x52: return 72 * 16384;  // 1.125MB
      case 0x53: return 80 * 16384;  // 1.25MB
      case 0x54: return 96 * 16384;  // 1.5MB
      default: return 32768;  // Default 32KB
    }
  }
}

uint16_t gb_get_rom_banks(uint8_t romSize) {
  if (romSize < 8) {
    return 1 << (romSize + 1);
  } else {
    // Special cases
    switch(romSize) {
      case 0x52: return 72;
      case 0x53: return 80;
      case 0x54: return 96;
      default: return 2;
    }
  }
}

const char* gb_get_rom_size_string(uint8_t romSize) {
  switch(romSize) {
    case 0: return "32KB";
    case 1: return "64KB";
    case 2: return "128KB";
    case 3: return "256KB";
    case 4: return "512KB";
    case 5: return "1MB";
    case 6: return "2MB";
    case 7: return "4MB";
    case 8: return "8MB";
    case 0x52: return "1.125MB";
    case 0x53: return "1.25MB";
    case 0x54: return "1.5MB";
    default: return "Unknown";
  }
}

// Standard GB cart operations
void gb_switch_bank(uint16_t bank) {
  // Ensure data pins are output
  dataOut_GB();
  
  // For MBC5, we can use full 9-bit bank number
  if (romType >= 0x19 && romType <= 0x1E) {
    writeByte_GB(0x2000, bank & 0xFF);      // Lower 8 bits
    writeByte_GB(0x3000, (bank >> 8) & 1);  // Bit 9
  }
  // For other MBCs, use standard bank switching
  else {
    writeByte_GB(0x2100, bank & 0xFF);
  }
}

void gb_reset_banks(void) {
  dataOut_GB();
  writeByte_GB(0x2000, 0);  // Bank low
  writeByte_GB(0x3000, 0);  // Bank high (MBC5)
  writeByte_GB(0x4000, 0);  // RAM bank
  writeByte_GB(0x6000, 0);  // ROM/RAM mode
  dataIn_GB();
}

//******************************************
// End of File
//******************************************