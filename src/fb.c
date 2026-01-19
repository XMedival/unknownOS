#include <fb.h>
#include <font.h>
#include <serial.h>
#include <multiboot2.h>
#include <string.h>
#include <log.h>
#include <input.h>
#include <kb.h>

// Display mode selection:
//   0 = Auto-detect (use whatever GRUB provides)
//   1 = Force VGA text mode (80x25)
//   2 = Force RGB framebuffer mode
#ifndef FB_FORCE_MODE
#define FB_FORCE_MODE 0
#endif

// Global framebuffer state
struct fb_info fb;

// Scroll buffer
static struct scroll_buffer scroll_buf;

// Current line buffer (accumulates characters until newline)
static char line_buf[SCROLL_LINE_LEN];
static uint line_pos = 0;

// Forward declarations
static void fb_init_text_mode(void);
static void fb_init_rgb_mode(struct multiboot_tag_framebuffer *tag);
static void fb_draw_char(uint col, uint row, uchar c, uint fg, uint bg);
static void fb_scroll(void);
static uint fb_color_to_pixel(uint rgb24);
static void fb_scroll_add_line(const char *text, uint len);
static void fb_redraw_from_buffer(void);
static void scroll_input_handler(struct key_event *ev, void *ctx);
static void fb_draw_scroll_line_at_row(int line_idx, uint row);

// External multiboot info structure
struct multiboot_info {
    uint total_size;
    uint reserved;
    struct multiboot_tag tags[0];
};

void fb_init(struct multiboot_info *mbi) {
    struct multiboot_tag_framebuffer *fb_tag = NULL;

    // Search for framebuffer tag using proper byte-based iteration
    struct multiboot_tag *tag = (struct multiboot_tag *)((uchar *)mbi + 8);
    while (tag->type != 0) {
        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            fb_tag = (struct multiboot_tag_framebuffer *)tag;
            break;
        }
        // Move to next tag (8-byte aligned)
        uint size = tag->size;
        uint advance = (size + 7) & ~7;
        if (advance == 0) advance = 8;
        tag = (struct multiboot_tag *)((uchar *)tag + advance);
    }
    // Check for forced mode
#if FB_FORCE_MODE == 1
    // Force VGA text mode unconditionally
    fb_init_text_mode();
    return;
#elif FB_FORCE_MODE == 2
    // Force RGB framebuffer - need valid tag
    if (fb_tag && fb_tag->common.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
        if ((fb_tag->common.framebuffer_addr >> 32) == 0) {
            fb_init_rgb_mode(fb_tag);
            return;
        }
    }
    fb_init_text_mode();
    LOG_WARN("RGB framebuffer not available, falling back to text mode");
    return;
#else
    // Auto-detect mode based on what GRUB provides
    if (!fb_tag) {
        fb_init_text_mode();
        return;
    }

    switch (fb_tag->common.framebuffer_type) {
    case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
        fb_init_text_mode();
        return;

    case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
        if ((fb_tag->common.framebuffer_addr >> 32) != 0) {
            fb_init_text_mode();
            return;
        }
        fb_init_rgb_mode(fb_tag);
        return;

    case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
        fb_init_text_mode();
        return;

    default:
        fb_init_text_mode();
        return;
    }
#endif
}

static void fb_init_text_mode(void) {
    fb.mode = FB_MODE_TEXT;
    fb.addr = (volatile uchar *)0xB8000;
    fb.width = 80;
    fb.height = 25;
    fb.pitch = 80 * 2;
    fb.bpp = 16;

    fb.text_cols = 80;
    fb.text_rows = 25;
    fb.cursor_x = 0;
    fb.cursor_y = 0;
    fb.fg_color = 7;    // VGA light gray
    fb.bg_color = 0;    // VGA black

    // Initialize scroll buffer now that dimensions are set
    fb_scroll_init();

    LOG_OK("display (VGA text 80x25)");
}

static void fb_init_rgb_mode(struct multiboot_tag_framebuffer *tag) {
    fb.mode = FB_MODE_RGB;
    fb.addr = (volatile uchar *)(uintptr_t)tag->common.framebuffer_addr;
    fb.pitch = tag->common.framebuffer_pitch;
    fb.width = tag->common.framebuffer_width;
    fb.height = tag->common.framebuffer_height;
    fb.bpp = tag->common.framebuffer_bpp;

    fb.red_pos = tag->framebuffer_red_field_position;
    fb.red_mask = tag->framebuffer_red_mask_size;
    fb.green_pos = tag->framebuffer_green_field_position;
    fb.green_mask = tag->framebuffer_green_mask_size;
    fb.blue_pos = tag->framebuffer_blue_field_position;
    fb.blue_mask = tag->framebuffer_blue_mask_size;

    // Calculate text dimensions
    fb.text_cols = fb.width / FONT_WIDTH;
    fb.text_rows = fb.height / FONT_HEIGHT;
    fb.cursor_x = 0;
    fb.cursor_y = 0;
    fb.fg_color = FB_COLOR_WHITE;
    fb.bg_color = FB_COLOR_BLACK;

    // Clear screen
    fb_clear();

    // Initialize scroll buffer now that dimensions are set
    fb_scroll_init();

    LOG_OK("display (framebuffer %dx%d @ %dbpp)", fb.width, fb.height, fb.bpp);
}

// Pack RGB color into pixel format
static uint fb_color_to_pixel(uint rgb24) {
    uint r = (rgb24 >> 16) & 0xFF;
    uint g = (rgb24 >> 8) & 0xFF;
    uint b = rgb24 & 0xFF;

    // Shift and mask each component to fit the framebuffer format
    r = (r >> (8 - fb.red_mask)) & ((1 << fb.red_mask) - 1);
    g = (g >> (8 - fb.green_mask)) & ((1 << fb.green_mask) - 1);
    b = (b >> (8 - fb.blue_mask)) & ((1 << fb.blue_mask) - 1);

    return (r << fb.red_pos) | (g << fb.green_pos) | (b << fb.blue_pos);
}

void fb_putpixel(uint x, uint y, uint color) {
    if (fb.mode != FB_MODE_RGB || x >= fb.width || y >= fb.height)
        return;

    uint pixel = fb_color_to_pixel(color);
    uint offset = y * fb.pitch + x * (fb.bpp / 8);

    switch (fb.bpp) {
    case 32:
        *(uint *)(fb.addr + offset) = pixel;
        break;
    case 24:
        fb.addr[offset] = pixel & 0xFF;
        fb.addr[offset + 1] = (pixel >> 8) & 0xFF;
        fb.addr[offset + 2] = (pixel >> 16) & 0xFF;
        break;
    case 16:
        *(ushort *)(fb.addr + offset) = (ushort)pixel;
        break;
    }
}

void fb_fill_rect(uint x, uint y, uint w, uint h, uint color) {
    if (fb.mode != FB_MODE_RGB)
        return;

    uint pixel = fb_color_to_pixel(color);

    for (uint row = y; row < y + h && row < fb.height; row++) {
        uint offset = row * fb.pitch + x * (fb.bpp / 8);
        for (uint col = 0; col < w && x + col < fb.width; col++) {
            switch (fb.bpp) {
            case 32:
                *(uint *)(fb.addr + offset) = pixel;
                offset += 4;
                break;
            case 24:
                fb.addr[offset] = pixel & 0xFF;
                fb.addr[offset + 1] = (pixel >> 8) & 0xFF;
                fb.addr[offset + 2] = (pixel >> 16) & 0xFF;
                offset += 3;
                break;
            case 16:
                *(ushort *)(fb.addr + offset) = (ushort)pixel;
                offset += 2;
                break;
            }
        }
    }
}

// Draw a single character at text position (col, row)
static void fb_draw_char(uint col, uint row, uchar c, uint fg, uint bg) {
    if (fb.mode == FB_MODE_TEXT) {
        // VGA text mode - write character + attribute
        uint offset = (row * fb.text_cols + col) * 2;
        fb.addr[offset] = c;
        fb.addr[offset + 1] = ((bg & 0x07) << 4) | (fg & 0x0F);
        return;
    }

    // Graphics mode - render using bitmap font
    uint px = col * FONT_WIDTH;
    uint py = row * FONT_HEIGHT;
    const uchar *glyph = font_8x16[c];

    for (uint y = 0; y < FONT_HEIGHT; y++) {
        uchar bits = glyph[y];
        for (uint x = 0; x < FONT_WIDTH; x++) {
            uint color = (bits & (0x80 >> x)) ? fg : bg;
            fb_putpixel(px + x, py + y, color);
        }
    }
}

// Scroll screen up by one line - redraw from scroll buffer (no slow FB reads)
static void fb_scroll(void) {
    // Just redraw everything from the scroll buffer
    // This is faster than copying pixels because it only writes to FB, never reads
    fb_redraw_from_buffer();
}

void fb_putchar(int c) {
    // Always send to serial
    if (c == '\n') {
        serial_putc('\r');
        serial_putc('\n');
    } else if (c != '\r') {
        serial_putc(c);
    }

    // Auto-return to live view on new output
    if (scroll_buf.view_offset > 0) {
        fb_scroll_to_bottom();
    }

    // Handle special characters
    if (c == '\n') {
        // Save completed line to scroll buffer
        line_buf[line_pos] = '\0';
        fb_scroll_add_line(line_buf, line_pos);
        line_pos = 0;

        fb.cursor_x = 0;
        fb.cursor_y++;
        if (fb.cursor_y >= fb.text_rows) {
            fb.cursor_y = fb.text_rows - 1;
            fb_scroll();
        }
        return;
    }

    if (c == '\r') {
        fb.cursor_x = 0;
        line_pos = 0;
        return;
    }

    if (c == '\t') {
        // Tab to next 8-column boundary
        do {
            fb_putchar(' ');
        } while (fb.cursor_x % 8 != 0);
        return;
    }

    if (c == '\b') {
        if (fb.cursor_x > 0) {
            fb.cursor_x--;
            fb_draw_char(fb.cursor_x, fb.cursor_y, ' ', fb.fg_color, fb.bg_color);
            if (line_pos > 0) line_pos--;
        }
        return;
    }

    // Track character in line buffer
    if (line_pos < SCROLL_LINE_LEN - 1) {
        line_buf[line_pos++] = (char)c;
    }

    // Draw character
    fb_draw_char(fb.cursor_x, fb.cursor_y, (uchar)c, fb.fg_color, fb.bg_color);
    fb.cursor_x++;

    // Line wrap
    if (fb.cursor_x >= fb.text_cols) {
        // Save wrapped line to scroll buffer
        line_buf[line_pos] = '\0';
        fb_scroll_add_line(line_buf, line_pos);
        line_pos = 0;

        fb.cursor_x = 0;
        fb.cursor_y++;
        if (fb.cursor_y >= fb.text_rows) {
            fb.cursor_y = fb.text_rows - 1;
            fb_scroll();
        }
    }
}

void fb_puts(const char *s) {
    while (*s) {
        fb_putchar(*s++);
    }
}

void fb_clear(void) {
    if (fb.mode == FB_MODE_TEXT) {
        for (uint i = 0; i < fb.text_cols * fb.text_rows * 2; i += 2) {
            fb.addr[i] = ' ';
            fb.addr[i + 1] = 0x07;
        }
    } else {
        // Clear entire visible text area
        fb_fill_rect(0, 0, fb.text_cols * FONT_WIDTH,
                     fb.text_rows * FONT_HEIGHT, fb.bg_color);
    }
    fb.cursor_x = 0;
    fb.cursor_y = 0;
}

void fb_set_colors(uint fg, uint bg) {
    fb.fg_color = fg;
    fb.bg_color = bg;
}

int fb_is_graphics_mode(void) {
    return fb.mode == FB_MODE_RGB;
}

// Scroll buffer implementation

void fb_scroll_init(void) {
    // Clear scroll buffer
    scroll_buf.write_idx = 0;
    scroll_buf.total_lines = 0;
    scroll_buf.view_offset = 0;

    for (uint i = 0; i < SCROLL_LINES; i++) {
        scroll_buf.lines[i].len = 0;
        scroll_buf.lines[i].text[0] = '\0';
    }

    // Clear line buffer
    line_pos = 0;
    line_buf[0] = '\0';

    // Register scroll key handler
    input_register(scroll_input_handler, NULL, 0);
}

// Add a completed line to scroll buffer
static void fb_scroll_add_line(const char *text, uint len) {
    struct scroll_line *line = &scroll_buf.lines[scroll_buf.write_idx];

    // Copy text
    uint copy_len = (len < SCROLL_LINE_LEN - 1) ? len : SCROLL_LINE_LEN - 1;
    for (uint i = 0; i < copy_len; i++) {
        line->text[i] = text[i];
    }
    line->text[copy_len] = '\0';
    line->len = copy_len;
    line->fg_color = fb.fg_color;
    line->bg_color = fb.bg_color;

    // Advance write position (circular)
    scroll_buf.write_idx = (scroll_buf.write_idx + 1) % SCROLL_LINES;

    // Track total lines
    if (scroll_buf.total_lines < SCROLL_LINES) {
        scroll_buf.total_lines++;
    }
}

// Get line from scroll buffer (0 = oldest visible, total_lines-1 = newest)
static struct scroll_line *fb_scroll_get_line(uint idx) {
    if (idx >= scroll_buf.total_lines) {
        return NULL;
    }

    // Calculate actual index in circular buffer
    // write_idx points to next write position
    // So oldest line is at (write_idx - total_lines + SCROLL_LINES) % SCROLL_LINES
    uint oldest_idx = (scroll_buf.write_idx + SCROLL_LINES - scroll_buf.total_lines) % SCROLL_LINES;
    uint actual_idx = (oldest_idx + idx) % SCROLL_LINES;

    return &scroll_buf.lines[actual_idx];
}

// Draw a specific line from scroll buffer at a specific screen row
static void fb_draw_scroll_line_at_row(int line_idx, uint row) {
    if (line_idx < 0 || (uint)line_idx >= scroll_buf.total_lines) {
        // Clear the row if no line
        if (fb.mode == FB_MODE_TEXT) {
            uint line_bytes = fb.text_cols * 2;
            for (uint j = 0; j < line_bytes; j += 2) {
                fb.addr[row * line_bytes + j] = ' ';
                fb.addr[row * line_bytes + j + 1] = 0x07;
            }
        } else {
            fb_fill_rect(0, row * FONT_HEIGHT, fb.text_cols * FONT_WIDTH, FONT_HEIGHT, fb.bg_color);
        }
        return;
    }

    struct scroll_line *line = fb_scroll_get_line(line_idx);
    if (!line) return;

    // Clear the row first
    if (fb.mode == FB_MODE_TEXT) {
        uint line_bytes = fb.text_cols * 2;
        for (uint j = 0; j < line_bytes; j += 2) {
            fb.addr[row * line_bytes + j] = ' ';
            fb.addr[row * line_bytes + j + 1] = 0x07;
        }
    } else {
        fb_fill_rect(0, row * FONT_HEIGHT, fb.text_cols * FONT_WIDTH, FONT_HEIGHT, fb.bg_color);
    }

    // Draw characters
    for (uint col = 0; col < line->len && col < fb.text_cols; col++) {
        fb_draw_char(col, row, line->text[col], line->fg_color, line->bg_color);
    }
}

// Redraw screen from scroll buffer (no clear - each line handles its own row)
static void fb_redraw_from_buffer(void) {
    uint display_rows = fb.text_rows;
    int start_line;

    if (scroll_buf.view_offset == 0) {
        start_line = (int)scroll_buf.total_lines - (int)display_rows;
        if (start_line < 0) start_line = 0;
    } else {
        start_line = (int)scroll_buf.total_lines - (int)display_rows - scroll_buf.view_offset;
        if (start_line < 0) start_line = 0;
    }

    // Draw all lines (each line clears its own row)
    for (uint row = 0; row < display_rows; row++) {
        fb_draw_scroll_line_at_row(start_line + row, row);
    }

    // If in live view, also show the current incomplete line
    if (scroll_buf.view_offset == 0 && line_pos > 0) {
        uint row = fb.cursor_y;
        // Clear rest of current line and draw partial content
        if (fb.mode == FB_MODE_TEXT) {
            uint line_bytes = fb.text_cols * 2;
            uint base = row * line_bytes;
            for (uint col = 0; col < fb.text_cols; col++) {
                if (col < line_pos) {
                    fb.addr[base + col * 2] = line_buf[col];
                    fb.addr[base + col * 2 + 1] = 0x07;
                } else {
                    fb.addr[base + col * 2] = ' ';
                    fb.addr[base + col * 2 + 1] = 0x07;
                }
            }
        } else {
            for (uint col = 0; col < fb.text_cols; col++) {
                if (col < line_pos) {
                    fb_draw_char(col, row, line_buf[col], fb.fg_color, fb.bg_color);
                } else {
                    fb_draw_char(col, row, ' ', fb.fg_color, fb.bg_color);
                }
            }
        }
    }
}

// Prevent re-entrant scroll during redraw
static volatile int scroll_busy = 0;

void fb_scroll_up(uint n) {
    if (scroll_busy) return;
    if (scroll_buf.total_lines == 0) return;
    if (n == 0) return;

    scroll_busy = 1;

    int max_offset = (int)scroll_buf.total_lines - (int)fb.text_rows;
    if (max_offset < 0) max_offset = 0;

    uint old_offset = scroll_buf.view_offset;
    scroll_buf.view_offset += n;
    if (scroll_buf.view_offset > (uint)max_offset) {
        scroll_buf.view_offset = max_offset;
    }

    if (scroll_buf.view_offset != old_offset) {
        fb_redraw_from_buffer();
    }

    scroll_busy = 0;
}

void fb_scroll_down(uint n) {
    if (scroll_busy) return;
    if (scroll_buf.view_offset == 0) return;
    if (n == 0) return;

    scroll_busy = 1;

    uint old_offset = scroll_buf.view_offset;
    if (n >= scroll_buf.view_offset) {
        scroll_buf.view_offset = 0;
    } else {
        scroll_buf.view_offset -= n;
    }

    if (scroll_buf.view_offset != old_offset) {
        fb_redraw_from_buffer();
    }

    scroll_busy = 0;
}

void fb_scroll_to_bottom(void) {
    if (scroll_buf.view_offset == 0) return;

    scroll_buf.view_offset = 0;
    fb_redraw_from_buffer();
}

int fb_is_scrolled(void) {
    return scroll_buf.view_offset > 0;
}

// Public drawing functions for VT subsystem
void fb_draw_char_at(uint col, uint row, int c, uint fg, uint bg) {
    fb_draw_char(col, row, (uchar)c, fg, bg);
}

void fb_get_text_dimensions(uint *cols, uint *rows) {
    if (cols) *cols = fb.text_cols;
    if (rows) *rows = fb.text_rows;
}

// Check if VT system is active (from vt.c)
struct vt_struct;
extern struct vt_struct *active_vt;

// Get scroll buffer line count for VT migration
uint fb_get_scroll_line_count(void) {
    return scroll_buf.total_lines;
}

// Get a line from scroll buffer (returns length, -1 if invalid)
int fb_get_scroll_line(uint idx, char *buf, uint bufsize, uint *fg, uint *bg) {
    struct scroll_line *line = fb_scroll_get_line(idx);
    if (!line) return -1;

    uint len = line->len;
    if (len >= bufsize) len = bufsize - 1;

    for (uint i = 0; i < len; i++) {
        buf[i] = line->text[i];
    }
    buf[len] = '\0';

    if (fg) *fg = line->fg_color;
    if (bg) *bg = line->bg_color;

    return (int)line->len;
}

// Scroll key handler
static void scroll_input_handler(struct key_event *ev, void *ctx) {
    (void)ctx;

    if (!ev->pressed) return;

    // If VT system is active, let it handle scrolling
    if (active_vt) return;

    switch (ev->keycode) {
    case KEY_PGUP:
        fb_scroll_up(fb.text_rows > 1 ? fb.text_rows - 1 : 1);
        break;

    case KEY_PGDN:
        fb_scroll_down(fb.text_rows > 1 ? fb.text_rows - 1 : 1);
        break;

    case KEY_UP:
        if (ev->flags & MOD_SHIFT) {
            fb_scroll_up(1);
        }
        break;

    case KEY_DOWN:
        if (ev->flags & MOD_SHIFT) {
            fb_scroll_down(1);
        }
        break;

    case KEY_HOME:
        if (ev->flags & MOD_SHIFT) {
            // Scroll to top
            fb_scroll_up(scroll_buf.total_lines);
        }
        break;

    case KEY_END:
        if (ev->flags & MOD_SHIFT) {
            fb_scroll_to_bottom();
        }
        break;
    }
}
