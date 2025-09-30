#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

// OLED Display Configuration
// 128 pixels wide / 6 pixels per char = 21 characters
#define OLED_CHAR_WIDTH     21      // Maximum characters per line
#define OLED_CHAR_BUFFER    (OLED_CHAR_WIDTH + 1)  // Buffer size with null terminator
#define OLED_DISPLAY_LINES  8       // Number of lines on display

// Progress bar configuration
#define PROGRESS_BAR_WIDTH  (OLED_CHAR_WIDTH - 2)  // Width minus brackets

#endif // DISPLAY_CONFIG_H