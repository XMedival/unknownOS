#pragma once
#include <types.h>

// Display mode types
#define FB_MODE_TEXT     0   // VGA text mode (0xB8000)
#define FB_MODE_RGB      1   // Linear RGB framebuffer

// Font dimensions (standard VGA font)
#define FONT_WIDTH   8
#define FONT_HEIGHT  16

// Default text colors (RRGGBB format for RGB mode)
#define FB_COLOR_WHITE   0xFFFFFF
#define FB_COLOR_BLACK   0x000000
#define FB_COLOR_GRAY    0xAAAAAA
#define FB_COLOR_GREEN   0x00AA00

// Scroll buffer configuration
#define SCROLL_LINES     500   // Number of lines to keep in history
#define SCROLL_LINE_LEN  256   // Max characters per line

// Scroll line entry
struct scroll_line {
    char text[SCROLL_LINE_LEN];
    uint8_t len;
    uint fg_color;                // Color at time of write (for graphics mode)
    uint bg_color;
};

// Scroll buffer state
struct scroll_buffer {
    struct scroll_line lines[SCROLL_LINES];
    uint write_idx;               // Circular write position (next line to write)
    uint total_lines;             // Total lines written (max SCROLL_LINES)
    int view_offset;              // 0 = live view, >0 = scrolled back N lines
};

// Framebuffer state
struct fb_info {
    uint mode;                    // FB_MODE_TEXT or FB_MODE_RGB

    // Framebuffer memory
    volatile uchar *addr;         // Framebuffer base address
    uint pitch;                   // Bytes per scanline
    uint width;                   // Width in pixels
    uint height;                  // Height in pixels
    uint bpp;                     // Bits per pixel (8, 16, 24, 32)

    // RGB field positions (for RGB mode)
    uchar red_pos;
    uchar red_mask;
    uchar green_pos;
    uchar green_mask;
    uchar blue_pos;
    uchar blue_mask;

    // Text console state
    uint text_cols;               // Characters per row
    uint text_rows;               // Number of text rows
    uint cursor_x;                // Current column (0-based)
    uint cursor_y;                // Current row (0-based)
    uint fg_color;                // Foreground color
    uint bg_color;                // Background color
};

extern struct fb_info fb;

// Forward declaration
struct multiboot_info;

// Initialization
void fb_init(struct multiboot_info *mbi);

// Pixel operations (graphics mode only)
void fb_putpixel(uint x, uint y, uint color);
void fb_fill_rect(uint x, uint y, uint w, uint h, uint color);

// Text operations (both modes)
void fb_putchar(int c);
void fb_puts(const char *s);
void fb_clear(void);
void fb_set_colors(uint fg, uint bg);

// Mode query
int fb_is_graphics_mode(void);

// Scroll buffer operations
void fb_scroll_init(void);           // Initialize scroll buffer
void fb_scroll_up(uint n);           // Scroll back n lines
void fb_scroll_down(uint n);         // Scroll forward n lines
void fb_scroll_to_bottom(void);      // Return to live output
int fb_is_scrolled(void);            // Returns 1 if viewing history
