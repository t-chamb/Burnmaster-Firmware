#ifndef DISPLAYBUFFER_H
#define DISPLAYBUFFER_H

#include <stdint.h>
#include "display_config.h"

/*
 * DisplayBuffer - Dynamic display buffering system
 * 
 * Usage Examples:
 * 
 * 1. Terminal-style scrolling (for validation, logs, etc):
 *    DisplayBuffer_InitEx(DISPLAY_MODE_SCROLL, 0);
 *    DisplayBuffer_AddLine("Starting process...");
 *    DisplayBuffer_AddLine("Step 1 complete");
 *    DisplayBuffer_ForceUpdate();
 * 
 * 2. Menu/Status display (fixed positions):
 *    DisplayBuffer_InitEx(DISPLAY_MODE_STATIC, 1);
 *    DisplayBuffer_SetLine(0, "== Main Menu ==");
 *    DisplayBuffer_SetLine(2, "1. Read ROM");
 *    DisplayBuffer_SetLine(3, "2. Write ROM");
 * 
 * 3. Paged output (for long lists):
 *    DisplayBuffer_InitEx(DISPLAY_MODE_PAGED, 1);
 *    DisplayBuffer_SetFlushThreshold(8);  // Full page
 *    for(i = 0; i < items; i++) {
 *        DisplayBuffer_AddLine(item[i]);
 *    }
 */

// Display buffer configuration
#define DISPLAY_LINES OLED_DISPLAY_LINES
#define DISPLAY_WIDTH OLED_CHAR_WIDTH   // Display width in characters
#define LINE_BUFFER_SIZE OLED_CHAR_BUFFER  // Buffer size with null terminator
#define BUFFER_SIZE 16  // Can buffer up to 16 lines before display

// Display modes
typedef enum {
    DISPLAY_MODE_SCROLL,      // Terminal-style scrolling
    DISPLAY_MODE_PAGED,       // Clear screen between pages
    DISPLAY_MODE_STATIC       // Fixed positions, no scrolling
} display_mode_t;

// Display buffer structure
typedef struct {
    char screen[DISPLAY_LINES][LINE_BUFFER_SIZE];  // Current screen content
    char buffer[BUFFER_SIZE][LINE_BUFFER_SIZE];    // Buffered lines waiting to display
    uint8_t buffer_count;                           // Number of lines in buffer
    uint8_t buffer_head;                            // Head of circular buffer
    uint8_t buffer_tail;                            // Tail of circular buffer
    display_mode_t mode;                            // Current display mode
    uint8_t clear_on_init;                          // Whether to clear screen on init
    uint8_t auto_scroll;                            // Auto-scroll when screen is full
} DisplayBuffer_t;

// Initialize display buffer with options
void DisplayBuffer_Init(void);
void DisplayBuffer_InitEx(display_mode_t mode, uint8_t clear_screen);

// Add a line to the buffer
void DisplayBuffer_AddLine(const char* text);

// Add a line at specific position (for static mode)
void DisplayBuffer_SetLine(uint8_t line, const char* text);

// Flush all buffered lines to display
void DisplayBuffer_Flush(void);

// Clear the display buffer
void DisplayBuffer_Clear(void);

// Force immediate display update (bypasses buffering)
void DisplayBuffer_ForceUpdate(void);

// Set automatic flush threshold (0 = disabled)
void DisplayBuffer_SetFlushThreshold(uint8_t lines);

// Set display mode
void DisplayBuffer_SetMode(display_mode_t mode);

// Get current buffer count
uint8_t DisplayBuffer_GetPendingCount(void);

// Check if it's time to flush and do so if needed
void DisplayBuffer_PeriodicFlush(void);

// Set auto-scroll behavior
void DisplayBuffer_SetAutoScroll(uint8_t enabled);

#endif // DISPLAYBUFFER_H