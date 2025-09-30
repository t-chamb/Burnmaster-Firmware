#pragma onece

#include "display_config.h"

#define I2C1_SLAVE_ADDRESS7    0x78
#define SSD1306_ADDR 0x3c
#define MAX_COLUMN 128

#define LED1 (1)
#define LED_B (2)
#define LED_G (4)
#define LED_R (8)



void I2cInit(void);
void SSD1306_WriteCmd(uint8_t var);
void SSD1306_WriteData(uint8_t var);



//坐标设置：也就是在哪里显示
void OledSetPos(uint8_t x, uint8_t y);
//开启Oled显示
void OledDisplayOn(void);
//关闭Oled显示   
void OledDisplayOff(void);
//清屏函数,清完屏,整个屏幕是黑色的!和没点亮一样  
void OledClear(void);
//在指定位置显示一个字符,包括部分字符
//x:0~127，y:0~7
//Char_Size:选择字体 16/12 
void OledShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t Char_Size);
//显示一个字符串
uint8_t OledShowString(uint8_t x,uint8_t y,const char *str,uint8_t Char_Size);
//显示一个位图
void OledShowPicData(uint8_t x,uint8_t y,uint8_t wdt,uint8_t hgt,uint8_t *pPicData);
//初始化
void OledInit(void);



void setColor_RGB(uint8_t r, uint8_t g, uint8_t b);
void print_Error(char *errorMessage, uint8_t forceReset);
void draw_progressbar(uint32_t processed, uint32_t total, uint8_t line);
void showPersent(uint32_t processed, uint32_t total, uint8_t x, uint8_t line);

// New unified progress display system that handles screen clearing and text display
void progress_begin(const char* title, const char* status);
void progress_update(uint32_t processed, uint32_t total);
void progress_complete(const char* message);
void progress_set_status(const char* status);
void progress_error(const char* error_msg);

// Simple progress operations (replaces direct draw_progressbar calls)
void simple_progress_begin(const char* operation, uint32_t total);
void simple_progress_update(uint32_t current, uint32_t total);
void simple_progress_end(void);

// Display utilities
void display_message(uint8_t line, const char* message);
void display_clear_line(uint8_t line);
void display_error(const char* title, const char* error, uint8_t wait_for_button);

// Scrolling display functionality
void display_scroll_init(uint8_t start_line, uint8_t end_line);
void display_scroll_add_line(const char* message);
void display_scroll_clear(void);

// Full screen management
void display_save_screen(void);
void display_restore_screen(void);
void display_update_line(uint8_t line, const char* message);

// ROM validation display
void display_validation_start(void);
void display_checksum_result(const char* calculated, const char* expected, uint8_t matches);
void display_validation_complete(void);

// Compatibility macros for easier migration
#define PROGRESS_INIT(title, total) progress_begin(title, "Starting...")
#define PROGRESS_UPDATE(current, total) progress_update(current, total)
#define PROGRESS_DONE(msg) progress_complete(msg)
#define PROGRESS_STATUS(msg) progress_set_status(msg)

// Standard display lines for consistency
#define DISPLAY_LINE_TITLE    0
#define DISPLAY_LINE_STATUS   1
#define DISPLAY_LINE_PROGRESS 2
#define DISPLAY_LINE_PERCENT  3
#define DISPLAY_LINE_INFO     4
#define DISPLAY_LINE_ERROR    5
#define DISPLAY_LINE_RESULT   6
#define DISPLAY_LINE_PROMPT   7




//Leds
void LEDSInit();
void LED_ON(uint8_t LedNum);
void LED_OFF(uint8_t LedNum);
void LED_BLINK(uint8_t LedNum);
void LED_CLEAR(void);

#define LED_RED_ON LED_ON(LED_R)
#define LED_GREEN_ON LED_ON(LED_G)
#define LED_BLUE_ON LED_ON(LED_B)
#define LED_RED_OFF LED_OFF(LED_R)
#define LED_GREEN_OFF LED_OFF(LED_G)
#define LED_BLUE_OFF LED_OFF(LED_B)
#define LED_RED_BLINK LED_BLINK(LED_R)
#define LED_GREEN_BLINK LED_BLINK(LED_G)
#define LED_BLUE_BLINK LED_BLINK(LED_B)