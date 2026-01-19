#include <vt.h>
#include <tty.h>
#include <fb.h>
#include <input.h>
#include <kb.h>
#include <string.h>
#include <log.h>

// Global VT array and active VT
struct vt_struct vts[NR_VTS];
struct vt_struct *active_vt = NULL;

// Forward declarations
static void vt_input_handler(struct key_event *ev, void *ctx);
static void vt_scroll_line(struct vt_struct *vt);
static void vt_add_line_to_buffer(struct vt_struct *vt);
static struct vt_cell *vt_get_line(struct vt_struct *vt, uint idx);

void vt_init(void) {
    uint cols, rows;
    fb_get_text_dimensions(&cols, &rows);

    // Initialize all VTs
    for (int i = 0; i < NR_VTS; i++) {
        struct vt_struct *vt = &vts[i];
        memset(vt, 0, sizeof(struct vt_struct));

        vt->id = i;
        vt->active = (i == 0);  // VT0 is initially active
        vt->cols = cols;
        vt->rows = rows;
        vt->cursor_x = 0;
        vt->cursor_y = 0;
        vt->fg_color = FB_COLOR_WHITE;
        vt->bg_color = FB_COLOR_BLACK;
        vt->write_row = 0;
        vt->total_rows = 0;
        vt->view_offset = 0;
        vt->line_pos = 0;
        vt->tty = NULL;  // Will be linked by tty_init()
    }

    active_vt = &vts[0];

    // Copy existing fb scroll buffer to VT0 (preserve boot messages)
    uint fb_lines = fb_get_scroll_line_count();
    struct vt_struct *vt0 = &vts[0];

    for (uint i = 0; i < fb_lines && i < VT_ROWS; i++) {
        char linebuf[VT_COLS];
        uint fg, bg;
        int len = fb_get_scroll_line(i, linebuf, VT_COLS, &fg, &bg);

        if (len >= 0) {
            struct vt_cell *line = vt0->buffer[vt0->write_row];

            for (uint j = 0; j < VT_COLS; j++) {
                if (j < (uint)len) {
                    line[j].c = linebuf[j];
                    line[j].fg = 7;  // Default color
                    line[j].bg = 0;
                } else {
                    line[j].c = ' ';
                    line[j].fg = 7;
                    line[j].bg = 0;
                }
            }

            vt0->write_row = (vt0->write_row + 1) % VT_ROWS;
            vt0->total_rows++;
        }
    }

    // Set cursor to end of content
    vt0->cursor_y = (vt0->total_rows < vt0->rows) ? vt0->total_rows : vt0->rows - 1;

    // Register input handler for VT switching
    input_register(vt_input_handler, NULL, 0);

    LOG_OK("vt: %d virtual terminals initialized (%d lines copied)", NR_VTS, fb_lines);
}

struct vt_struct *vt_current(void) {
    return active_vt;
}

void vt_switch(int vt_num) {
    if (vt_num < 0 || vt_num >= NR_VTS)
        return;

    if (active_vt && active_vt->id == vt_num)
        return;  // Already on this VT

    // Deactivate old VT
    if (active_vt) {
        active_vt->active = 0;
    }

    // Activate new VT
    active_vt = &vts[vt_num];
    active_vt->active = 1;
    active_vt->view_offset = 0;  // Reset to live view

    // Redraw
    vt_redraw(active_vt);

    LOG_INFO("vt: switched to VT%d", vt_num);
}

// Get a line from the VT buffer (0 = oldest, total_rows-1 = newest)
static struct vt_cell *vt_get_line(struct vt_struct *vt, uint idx) {
    if (idx >= vt->total_rows)
        return NULL;

    // Calculate actual index in circular buffer
    uint oldest_idx = (vt->write_row + VT_ROWS - vt->total_rows) % VT_ROWS;
    uint actual_idx = (oldest_idx + idx) % VT_ROWS;

    return vt->buffer[actual_idx];
}

// Add current line to scroll buffer
static void vt_add_line_to_buffer(struct vt_struct *vt) {
    struct vt_cell *line = vt->buffer[vt->write_row];

    // Copy current line content
    for (uint i = 0; i < vt->line_pos && i < VT_COLS; i++) {
        line[i].c = vt->line_buf[i];
        line[i].fg = (vt->fg_color == FB_COLOR_WHITE) ? 7 : 15;  // Simplified
        line[i].bg = 0;
    }

    // Clear rest of line
    for (uint i = vt->line_pos; i < VT_COLS; i++) {
        line[i].c = ' ';
        line[i].fg = 7;
        line[i].bg = 0;
    }

    // Advance write position
    vt->write_row = (vt->write_row + 1) % VT_ROWS;

    if (vt->total_rows < VT_ROWS) {
        vt->total_rows++;
    }
}

// Scroll the visible screen up by one line
static void vt_scroll_line(struct vt_struct *vt) {
    // When VT is active, redraw from buffer
    if (vt->active) {
        vt_redraw(vt);
    }
}

void vt_putchar(struct vt_struct *vt, int c) {
    if (!vt) return;

    // Auto-return to live view on output
    if (vt->view_offset > 0) {
        vt->view_offset = 0;
    }

    // Handle special characters
    if (c == '\n') {
        // Save line to buffer
        vt_add_line_to_buffer(vt);
        vt->line_pos = 0;

        vt->cursor_x = 0;
        vt->cursor_y++;

        if (vt->cursor_y >= vt->rows) {
            vt->cursor_y = vt->rows - 1;
            vt_scroll_line(vt);
        }
        return;
    }

    if (c == '\r') {
        vt->cursor_x = 0;
        vt->line_pos = 0;
        return;
    }

    if (c == '\t') {
        do {
            vt_putchar(vt, ' ');
        } while (vt->cursor_x % 8 != 0);
        return;
    }

    if (c == '\b') {
        if (vt->cursor_x > 0) {
            vt->cursor_x--;
            if (vt->line_pos > 0) vt->line_pos--;
            if (vt->active) {
                fb_draw_char_at(vt->cursor_x, vt->cursor_y, ' ',
                               vt->fg_color, vt->bg_color);
            }
        }
        return;
    }

    // Track character in line buffer
    if (vt->line_pos < VT_COLS - 1) {
        vt->line_buf[vt->line_pos++] = (char)c;
    }

    // Draw character if VT is active
    if (vt->active) {
        fb_draw_char_at(vt->cursor_x, vt->cursor_y, c,
                       vt->fg_color, vt->bg_color);
    }

    vt->cursor_x++;

    // Line wrap
    if (vt->cursor_x >= vt->cols) {
        vt_add_line_to_buffer(vt);
        vt->line_pos = 0;

        vt->cursor_x = 0;
        vt->cursor_y++;

        if (vt->cursor_y >= vt->rows) {
            vt->cursor_y = vt->rows - 1;
            vt_scroll_line(vt);
        }
    }
}

void vt_puts(struct vt_struct *vt, const char *s) {
    while (*s) {
        vt_putchar(vt, *s++);
    }
}

void vt_clear(struct vt_struct *vt) {
    if (!vt) return;

    vt->cursor_x = 0;
    vt->cursor_y = 0;
    vt->line_pos = 0;

    // Clear visible area if active
    if (vt->active) {
        for (uint row = 0; row < vt->rows; row++) {
            for (uint col = 0; col < vt->cols; col++) {
                fb_draw_char_at(col, row, ' ', vt->fg_color, vt->bg_color);
            }
        }
    }
}

void vt_set_colors(struct vt_struct *vt, uint fg, uint bg) {
    if (!vt) return;
    vt->fg_color = fg;
    vt->bg_color = bg;
}

void vt_scroll_up(struct vt_struct *vt, uint n) {
    if (!vt || vt->total_rows == 0 || n == 0) return;

    int max_offset = (int)vt->total_rows - (int)vt->rows;
    if (max_offset < 0) max_offset = 0;

    vt->view_offset += n;
    if (vt->view_offset > (uint)max_offset) {
        vt->view_offset = max_offset;
    }

    if (vt->active) {
        vt_redraw(vt);
    }
}

void vt_scroll_down(struct vt_struct *vt, uint n) {
    if (!vt || vt->view_offset == 0 || n == 0) return;

    if (n >= vt->view_offset) {
        vt->view_offset = 0;
    } else {
        vt->view_offset -= n;
    }

    if (vt->active) {
        vt_redraw(vt);
    }
}

void vt_redraw(struct vt_struct *vt) {
    if (!vt || !vt->active) return;

    int start_line;

    if (vt->view_offset == 0) {
        // Live view - show last (rows) lines
        start_line = (int)vt->total_rows - (int)vt->rows;
        if (start_line < 0) start_line = 0;
    } else {
        // Scrolled back
        start_line = (int)vt->total_rows - (int)vt->rows - (int)vt->view_offset;
        if (start_line < 0) start_line = 0;
    }

    // Draw lines from buffer
    for (uint row = 0; row < vt->rows; row++) {
        int line_idx = start_line + row;

        if (line_idx >= 0 && (uint)line_idx < vt->total_rows) {
            struct vt_cell *line = vt_get_line(vt, line_idx);
            if (line) {
                for (uint col = 0; col < vt->cols; col++) {
                    uint fg = (line[col].fg == 7) ? vt->fg_color : FB_COLOR_WHITE;
                    uint bg = vt->bg_color;
                    fb_draw_char_at(col, row, line[col].c, fg, bg);
                }
                continue;
            }
        }

        // Clear row if no line
        for (uint col = 0; col < vt->cols; col++) {
            fb_draw_char_at(col, row, ' ', vt->fg_color, vt->bg_color);
        }
    }

    // Draw current incomplete line if in live view
    if (vt->view_offset == 0 && vt->line_pos > 0) {
        uint row = vt->cursor_y;
        for (uint col = 0; col < vt->cols; col++) {
            if (col < vt->line_pos) {
                fb_draw_char_at(col, row, vt->line_buf[col],
                               vt->fg_color, vt->bg_color);
            } else {
                fb_draw_char_at(col, row, ' ', vt->fg_color, vt->bg_color);
            }
        }
    }
}

// Input handler for VT switching and scrolling
static void vt_input_handler(struct key_event *ev, void *ctx) {
    (void)ctx;

    if (!ev->pressed) return;

    // VT switching: Alt+F1 through Alt+F8
    if (ev->flags & MOD_ALT) {
        int vt_num = -1;
        switch (ev->keycode) {
        case KEY_F1: vt_num = 0; break;
        case KEY_F2: vt_num = 1; break;
        case KEY_F3: vt_num = 2; break;
        case KEY_F4: vt_num = 3; break;
        case KEY_F5: vt_num = 4; break;
        case KEY_F6: vt_num = 5; break;
        case KEY_F7: vt_num = 6; break;
        case KEY_F8: vt_num = 7; break;
        }

        if (vt_num >= 0) {
            vt_switch(vt_num);
            return;
        }
    }

    // Per-VT scrolling (only when VT system is active)
    if (!active_vt) return;

    switch (ev->keycode) {
    case KEY_PGUP:
        // PgUp scrolls back (with or without Shift)
        vt_scroll_up(active_vt, active_vt->rows > 1 ? active_vt->rows - 1 : 1);
        break;

    case KEY_PGDN:
        // PgDn scrolls forward
        vt_scroll_down(active_vt, active_vt->rows > 1 ? active_vt->rows - 1 : 1);
        break;

    case KEY_UP:
        if (ev->flags & MOD_SHIFT) {
            vt_scroll_up(active_vt, 1);
        }
        break;

    case KEY_DOWN:
        if (ev->flags & MOD_SHIFT) {
            vt_scroll_down(active_vt, 1);
        }
        break;

    case KEY_HOME:
        if (ev->flags & MOD_SHIFT) {
            vt_scroll_up(active_vt, active_vt->total_rows);
        }
        break;

    case KEY_END:
        if (ev->flags & MOD_SHIFT) {
            active_vt->view_offset = 0;
            vt_redraw(active_vt);
        }
        break;
    }
}
