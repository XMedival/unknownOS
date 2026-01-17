#include <fb.h>
#include <font.h>
#include <serial.h>
#include <multiboot2.h>
#include <string.h>
#include <log.h>

// Global framebuffer state
struct fb_info fb;

// Forward declarations
static void fb_init_text_mode(void);
static void fb_init_rgb_mode(struct multiboot_tag_framebuffer *tag);
static void fb_draw_char(uint col, uint row, uchar c, uint fg, uint bg);
static void fb_scroll(void);
static uint fb_color_to_pixel(uint rgb24);

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

    // No framebuffer tag - use VGA text mode
    if (!fb_tag) {
        fb_init_text_mode();
        return;
    }

    // Check framebuffer type
    switch (fb_tag->common.framebuffer_type) {
    case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
        // GRUB set up text mode for us
        fb_init_text_mode();
        return;

    case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
        // Check if framebuffer address is accessible (32-bit limit)
        if ((fb_tag->common.framebuffer_addr >> 32) != 0) {
            LOG_WARN("Framebuffer above 4GB, using text mode");
            fb_init_text_mode();
            return;
        }
        fb_init_rgb_mode(fb_tag);
        return;

    case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
        LOG_WARN("Indexed framebuffer not supported, using text mode");
        fb_init_text_mode();
        return;

    default:
        fb_init_text_mode();
        return;
    }
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

    LOG_OK("display (VGA text 80x25)");
}

static void fb_init_rgb_mode(struct multiboot_tag_framebuffer *tag) {
    fb.mode = FB_MODE_RGB;
    fb.addr = (volatile uchar *)(uint)tag->common.framebuffer_addr;
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

// Scroll screen up by one line
static void fb_scroll(void) {
    if (fb.mode == FB_MODE_TEXT) {
        // VGA text mode - copy memory
        uint line_bytes = fb.text_cols * 2;
        for (uint i = 0; i < fb.text_rows - 1; i++) {
            for (uint j = 0; j < line_bytes; j++) {
                fb.addr[i * line_bytes + j] = fb.addr[(i + 1) * line_bytes + j];
            }
        }
        // Clear last line
        for (uint j = 0; j < line_bytes; j += 2) {
            fb.addr[(fb.text_rows - 1) * line_bytes + j] = ' ';
            fb.addr[(fb.text_rows - 1) * line_bytes + j + 1] = 0x07;
        }
        return;
    }

    // Graphics mode - copy scanlines
    uint bytes_per_text_line = FONT_HEIGHT * fb.pitch;
    uint total_lines = (fb.text_rows - 1) * FONT_HEIGHT;

    // Copy lines up
    for (uint y = 0; y < total_lines; y++) {
        for (uint x = 0; x < fb.pitch; x++) {
            fb.addr[y * fb.pitch + x] = fb.addr[(y + FONT_HEIGHT) * fb.pitch + x];
        }
    }

    // Clear last text line
    fb_fill_rect(0, (fb.text_rows - 1) * FONT_HEIGHT,
                 fb.text_cols * FONT_WIDTH, FONT_HEIGHT, fb.bg_color);
}

void fb_putchar(int c) {
    // Always send to serial
    if (c == '\n') {
        serial_putc('\r');
        serial_putc('\n');
    } else if (c != '\r') {
        serial_putc(c);
    }

    // Handle special characters
    if (c == '\n') {
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
        }
        return;
    }

    // Draw character
    fb_draw_char(fb.cursor_x, fb.cursor_y, (uchar)c, fb.fg_color, fb.bg_color);
    fb.cursor_x++;

    // Line wrap
    if (fb.cursor_x >= fb.text_cols) {
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
