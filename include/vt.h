#pragma once
#include <types.h>

// Forward declaration
struct tty_struct;

// Configuration
#define NR_VTS          8       // Number of virtual terminals (VT0-VT7)
#define VT_COLS         256     // Max columns per line in buffer
#define VT_ROWS         500     // Scroll history lines

// Character cell with attributes
struct vt_cell {
    char c;                     // Character
    uint8_t fg;                 // Foreground color index
    uint8_t bg;                 // Background color index
};

// Virtual terminal state
struct vt_struct {
    int id;                     // VT number (0-7)
    int active;                 // Is this VT currently displayed?

    // Screen dimensions (from framebuffer)
    uint cols;                  // Visible columns
    uint rows;                  // Visible rows

    // Cursor position
    uint cursor_x;              // Column (0-based)
    uint cursor_y;              // Row (0-based)

    // Current colors (24-bit RGB for graphics mode)
    uint fg_color;
    uint bg_color;

    // Screen buffer (circular)
    struct vt_cell buffer[VT_ROWS][VT_COLS];
    uint write_row;             // Current write row in buffer (circular)
    uint total_rows;            // Total rows written
    int view_offset;            // Scroll offset (0 = live view)

    // Current line being built
    char line_buf[VT_COLS];
    uint line_pos;

    // Associated TTY (back-pointer)
    struct tty_struct *tty;
};

// Global VT array and active VT pointer
extern struct vt_struct vts[NR_VTS];
extern struct vt_struct *active_vt;

// VT API
void vt_init(void);
void vt_switch(int vt_num);
void vt_putchar(struct vt_struct *vt, int c);
void vt_puts(struct vt_struct *vt, const char *s);
void vt_clear(struct vt_struct *vt);
void vt_set_colors(struct vt_struct *vt, uint fg, uint bg);
void vt_scroll_up(struct vt_struct *vt, uint n);
void vt_scroll_down(struct vt_struct *vt, uint n);
void vt_redraw(struct vt_struct *vt);

// Get current VT for output (used by putchar)
struct vt_struct *vt_current(void);
