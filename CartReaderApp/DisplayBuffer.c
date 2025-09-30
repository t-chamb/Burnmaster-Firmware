#include <string.h>
#include <stdio.h>
#include "DisplayBuffer.h"
#include "Display.h"
#include "soft_uart.h"

// Display buffer instance
static DisplayBuffer_t display_buffer = {0};

// Track what's currently shown on physical display
static char physical_display[DISPLAY_LINES][LINE_BUFFER_SIZE];

// Configuration
static uint8_t auto_flush_threshold = 8;  // Default: flush when we have 8 lines
static uint32_t last_update_time = 0;
static uint32_t system_ticks = 0;

// 30 FPS = 33ms between frames
// Assuming ~100MHz CPU and simple delay loop, adjust this value
static const uint32_t TICKS_PER_FRAME = 100000;  // Tune this for 33ms

// Forward declarations
static void update_display(void);
static void scroll_buffer_up(int lines);
static void flush_scroll_mode(void);
static void flush_paged_mode(void);

// Get system tick counter (called periodically)
static uint32_t get_system_ticks(void) {
    // Just return the current tick count, don't increment here
    return system_ticks;
}


// Initialize display buffer with default settings
void DisplayBuffer_Init(void) {
    DisplayBuffer_InitEx(DISPLAY_MODE_SCROLL, 0);  // Default: scrolling, no clear
}

// Initialize display buffer with options
void DisplayBuffer_InitEx(display_mode_t mode, uint8_t clear_screen) {
    display_buffer.buffer_count = 0;
    display_buffer.buffer_head = 0;
    display_buffer.buffer_tail = 0;
    display_buffer.mode = mode;
    display_buffer.clear_on_init = clear_screen;
    display_buffer.auto_scroll = 1;  // Default: auto-scroll enabled
    
    auto_flush_threshold = (mode == DISPLAY_MODE_STATIC) ? 0 : 1;  
    last_update_time = get_system_ticks();
    
    // Clear screen buffer
    memset(display_buffer.screen, 0, sizeof(display_buffer.screen));
    memset(physical_display, 0, sizeof(physical_display));
    
    // Clear physical display if requested
    if (clear_screen) {
        OledClear();
    }
    
    char msg[64];
    sprintf(msg, "DisplayBuffer: Init mode=%d, clear=%d\r\n", mode, clear_screen);
    soft_uart_send_string(msg);
}

// Add a line at specific position (for static mode)
void DisplayBuffer_SetLine(uint8_t line, const char* text) {
    if (!text || line >= DISPLAY_LINES) return;
    
    // Clear the line first
    memset(display_buffer.screen[line], 0, LINE_BUFFER_SIZE);
    
    // Copy new text
    strncpy(display_buffer.screen[line], text, DISPLAY_WIDTH);
    display_buffer.screen[line][DISPLAY_WIDTH] = '\0';
    
    // Update display immediately in static mode
    if (display_buffer.mode == DISPLAY_MODE_STATIC) {
        update_display();
    }
}

// Add a line to the buffer
void DisplayBuffer_AddLine(const char* text) {
    if (!text) return;
    
    // In static mode, use SetLine instead
    if (display_buffer.mode == DISPLAY_MODE_STATIC) {
        // Find first empty line
        for (int i = 0; i < DISPLAY_LINES; i++) {
            if (display_buffer.screen[i][0] == '\0') {
                DisplayBuffer_SetLine(i, text);
                return;
            }
        }
        // No empty line, don't add
        return;
    }
    
    char msg[64];
    sprintf(msg, "DisplayBuffer: Adding '%s' (count=%d)\r\n", text, display_buffer.buffer_count);
    soft_uart_send_string(msg);
    
    // Add to circular buffer
    if (display_buffer.buffer_count < BUFFER_SIZE) {
        // Ensure text is properly truncated and null-terminated
        memset(display_buffer.buffer[display_buffer.buffer_head], 0, LINE_BUFFER_SIZE);
        strncpy(display_buffer.buffer[display_buffer.buffer_head], text, DISPLAY_WIDTH);
        display_buffer.buffer[display_buffer.buffer_head][DISPLAY_WIDTH] = '\0';
        
        display_buffer.buffer_head = (display_buffer.buffer_head + 1) % BUFFER_SIZE;
        display_buffer.buffer_count++;
        
        sprintf(msg, "DisplayBuffer: Added, head=%d, count=%d\r\n", display_buffer.buffer_head, display_buffer.buffer_count);
        soft_uart_send_string(msg);
        
        // Increment ticks to simulate time passing between adds
        system_ticks += 10000;
        
        // Check if we should flush based on time or count
        uint32_t current_time = get_system_ticks();
        uint32_t elapsed = current_time - last_update_time;
        
        // Flush based on threshold or time
        if (auto_flush_threshold > 0 && display_buffer.buffer_count >= auto_flush_threshold) {
            // Immediate flush when threshold reached
            soft_uart_send_string("DisplayBuffer: Threshold flush\r\n");
            DisplayBuffer_Flush();
            last_update_time = current_time;
        }
        // Disable time-based flush since we're using manual control
        // else if (display_buffer.buffer_count > 0 && elapsed >= TICKS_PER_FRAME) {
        //     // Time-based flush for smoother updates
        //     soft_uart_send_string("DisplayBuffer: Time-based flush\r\n");
        //     DisplayBuffer_Flush();
        //     last_update_time = current_time;
        // } 
        // Emergency flush if buffer is completely full
        else if (display_buffer.buffer_count >= BUFFER_SIZE - 1) {
            soft_uart_send_string("DisplayBuffer: Emergency buffer flush\r\n");
            DisplayBuffer_Flush();
            last_update_time = current_time;
        }
    } else {
        soft_uart_send_string("DisplayBuffer: Buffer full, flushing\r\n");
        DisplayBuffer_Flush();
        DisplayBuffer_AddLine(text);  // Retry after flush
    }
}

// Internal function to update display with 8 lines
static void update_display(void) {
    // Only update lines that have changed
    for (int i = 0; i < DISPLAY_LINES; i++) {
        if (memcmp(display_buffer.screen[i], physical_display[i], LINE_BUFFER_SIZE) != 0) {
            // Line has changed, update it
            char safe_line[OLED_CHAR_BUFFER];
            
            // Fill entire line with spaces first to clear old content
            memset(safe_line, ' ', OLED_CHAR_WIDTH);
            safe_line[OLED_CHAR_WIDTH] = '\0';
            
            // Copy the text over the spaces
            int len = strlen(display_buffer.screen[i]);
            if (len > 0) {
                if (len > OLED_CHAR_WIDTH) len = OLED_CHAR_WIDTH;
                memcpy(safe_line, display_buffer.screen[i], len);
            }
            
            OledShowString(0, i, safe_line, 8);
            
            // Update physical display tracking
            memcpy(physical_display[i], display_buffer.screen[i], LINE_BUFFER_SIZE);
        }
    }
}

// Internal function to scroll screen buffer up by n lines
static void scroll_buffer_up(int lines) {
    if (lines <= 0 || lines > DISPLAY_LINES) return;
    
    // Shift existing lines up
    for (int i = 0; i < DISPLAY_LINES - lines; i++) {
        memcpy(display_buffer.screen[i], display_buffer.screen[i + lines], LINE_BUFFER_SIZE);
    }
    
    // Clear bottom lines
    for (int i = DISPLAY_LINES - lines; i < DISPLAY_LINES; i++) {
        memset(display_buffer.screen[i], 0, LINE_BUFFER_SIZE);
    }
}

// Flush buffered lines to display
void DisplayBuffer_Flush(void) {
    if (display_buffer.buffer_count == 0) return;
    
    switch (display_buffer.mode) {
        case DISPLAY_MODE_SCROLL:
            flush_scroll_mode();
            break;
            
        case DISPLAY_MODE_PAGED:
            flush_paged_mode();
            break;
            
        case DISPLAY_MODE_STATIC:
            // Static mode doesn't use buffering
            break;
    }
}

// Internal flush for scroll mode
static void flush_scroll_mode(void) {
    // Find first empty line or start scrolling if screen is full
    int next_line = -1;
    for (int i = 0; i < DISPLAY_LINES; i++) {
        if (display_buffer.screen[i][0] == '\0') {
            next_line = i;
            break;
        }
    }
    
    // Process all buffered lines
    while (display_buffer.buffer_count > 0) {
        char* line = display_buffer.buffer[display_buffer.buffer_tail];
        display_buffer.buffer_tail = (display_buffer.buffer_tail + 1) % BUFFER_SIZE;
        display_buffer.buffer_count--;
        
        if (next_line >= 0 && next_line < DISPLAY_LINES) {
            // Place line directly if there's space
            memset(display_buffer.screen[next_line], 0, LINE_BUFFER_SIZE);
            strncpy(display_buffer.screen[next_line], line, DISPLAY_WIDTH);
            display_buffer.screen[next_line][DISPLAY_WIDTH] = '\0';
            next_line++;
        } else if (display_buffer.auto_scroll) {
            // Screen is full, scroll up
            scroll_buffer_up(1);
            memset(display_buffer.screen[DISPLAY_LINES - 1], 0, LINE_BUFFER_SIZE);
            strncpy(display_buffer.screen[DISPLAY_LINES - 1], line, DISPLAY_WIDTH);
            display_buffer.screen[DISPLAY_LINES - 1][DISPLAY_WIDTH] = '\0';
        }
    }
    
    // Update display once for all lines processed
    update_display();
    
    // Small delay to prevent too rapid updates
    for(volatile int i = 0; i < 50000; i++);
}

// Internal flush for paged mode
static void flush_paged_mode(void) {
    // Clear screen and display all buffered content
    OledClear();
    memset(physical_display, 0, sizeof(physical_display));
    
    int line_num = 0;
    while (display_buffer.buffer_count > 0 && line_num < DISPLAY_LINES) {
        char* line = display_buffer.buffer[display_buffer.buffer_tail];
        display_buffer.buffer_tail = (display_buffer.buffer_tail + 1) % BUFFER_SIZE;
        display_buffer.buffer_count--;
        
        memset(display_buffer.screen[line_num], 0, LINE_BUFFER_SIZE);
        strncpy(display_buffer.screen[line_num], line, DISPLAY_WIDTH);
        display_buffer.screen[line_num][DISPLAY_WIDTH] = '\0';
        line_num++;
    }
    
    update_display();
}

// Clear the display buffer
void DisplayBuffer_Clear(void) {
    memset(&display_buffer, 0, sizeof(display_buffer));
    OledClear();
}

// Force immediate display update (bypasses buffering)
void DisplayBuffer_ForceUpdate(void) {
    soft_uart_send_string("DisplayBuffer: ForceUpdate called\r\n");
    DisplayBuffer_Flush();
}

// Set automatic flush threshold
void DisplayBuffer_SetFlushThreshold(uint8_t lines) {
    auto_flush_threshold = lines;
    
    // If we already have enough lines, flush now
    if (auto_flush_threshold > 0 && display_buffer.buffer_count >= auto_flush_threshold) {
        DisplayBuffer_Flush();
    }
}

// Get current buffer count
uint8_t DisplayBuffer_GetPendingCount(void) {
    return display_buffer.buffer_count;
}

// Check if it's time to flush and do so if needed
void DisplayBuffer_PeriodicFlush(void) {
    if (display_buffer.buffer_count == 0) return;
    
    // Increment system ticks to simulate time passing
    system_ticks += 5000;
    uint32_t current_time = get_system_ticks();
    uint32_t elapsed = current_time - last_update_time;
    
    // Flush if enough time has passed (30 FPS)
    if (elapsed >= TICKS_PER_FRAME) {
        DisplayBuffer_Flush();
        last_update_time = current_time;
    }
}

// Set display mode
void DisplayBuffer_SetMode(display_mode_t mode) {
    display_buffer.mode = mode;
    
    // Adjust auto-flush based on mode
    if (mode == DISPLAY_MODE_STATIC) {
        auto_flush_threshold = 0;  // No auto-flush in static mode
    }
}

// Set auto-scroll behavior
void DisplayBuffer_SetAutoScroll(uint8_t enabled) {
    display_buffer.auto_scroll = enabled;
}