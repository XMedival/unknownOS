#include <tty.h>
#include <vt.h>
#include <input.h>
#include <kb.h>
#include <string.h>
#include <log.h>
#include <serial.h>

// Global TTY array
struct tty_struct ttys[NR_TTYS];

// Forward declarations
static void tty_input_handler(struct key_event *ev, void *ctx);
static void tty_echo(struct tty_struct *tty, char c);
static void tty_output_char(struct tty_struct *tty, char c);

void tty_init(void) {
    // Initialize all TTYs
    for (int i = 0; i < NR_TTYS; i++) {
        struct tty_struct *tty = &ttys[i];
        memset(tty, 0, sizeof(struct tty_struct));

        tty->index = i;
        tty->vt = &vts[i];          // Link to corresponding VT
        vts[i].tty = tty;           // Back-link VT to TTY

        // Default line discipline: echo + canonical mode
        tty->lflags = L_ECHO | L_ICANON | L_ICRNL;

        tty->input_head = 0;
        tty->input_tail = 0;
        tty->input_count = 0;
        tty->canon_len = 0;
        tty->canon_ready = 0;
        tty->open_count = 0;
    }

    // Register keyboard input handler
    input_register(tty_input_handler, NULL, 0);

    LOG_OK("tty: %d terminals initialized", NR_TTYS);
}

struct tty_struct *tty_get(int index) {
    if (index < 0 || index >= NR_TTYS)
        return NULL;
    return &ttys[index];
}

struct tty_struct *tty_current(void) {
    if (active_vt && active_vt->tty)
        return active_vt->tty;
    return &ttys[0];  // Default to tty0
}

int tty_open(struct tty_struct *tty) {
    if (!tty) return -1;
    tty->open_count++;
    return 0;
}

int tty_close(struct tty_struct *tty) {
    if (!tty) return -1;
    if (tty->open_count > 0)
        tty->open_count--;
    return 0;
}

// Output a character to the TTY (to VT and serial)
static void tty_output_char(struct tty_struct *tty, char c) {
    // Serial output
    if (c == '\n') {
        serial_putc('\r');
        serial_putc('\n');
    } else {
        serial_putc(c);
    }

    // VT output
    if (tty->vt) {
        vt_putchar(tty->vt, c);
    }
}

// Echo character back to terminal
static void tty_echo(struct tty_struct *tty, char c) {
    if (!(tty->lflags & L_ECHO))
        return;

    if (c == C_ERASE || c == '\b') {
        // Backspace: erase character
        tty_output_char(tty, '\b');
        tty_output_char(tty, ' ');
        tty_output_char(tty, '\b');
    } else if (c < ' ' && c != '\n' && c != '\t' && c != '\r') {
        // Control character: show as ^X
        tty_output_char(tty, '^');
        tty_output_char(tty, c + '@');
    } else {
        tty_output_char(tty, c);
    }
}

// Add character to input buffer
static void tty_add_to_buffer(struct tty_struct *tty, char c) {
    if (tty->input_count >= TTY_INPUT_SIZE)
        return;  // Buffer full

    tty->input_buf[tty->input_head] = c;
    tty->input_head = (tty->input_head + 1) % TTY_INPUT_SIZE;
    tty->input_count++;
}

// Process input character through line discipline
void tty_input_char(struct tty_struct *tty, char c) {
    if (!tty) return;

    // CR to NL translation
    if ((tty->lflags & L_ICRNL) && c == '\r') {
        c = '\n';
    }

    // Canonical mode processing
    if (tty->lflags & L_ICANON) {
        // Handle special characters
        if (c == C_ERASE || c == '\b' || c == 0x7F) {
            // Backspace
            if (tty->canon_len > 0) {
                tty->canon_len--;
                tty_echo(tty, '\b');
            }
            return;
        }

        if (c == C_KILL) {
            // Kill line - erase entire line
            while (tty->canon_len > 0) {
                tty->canon_len--;
                tty_echo(tty, '\b');
            }
            return;
        }

        if (c == C_EOF) {
            // EOF - make whatever is in buffer available
            tty->canon_ready = 1;
            return;
        }

        // Add character to canonical buffer
        if (tty->canon_len < TTY_INPUT_SIZE - 1) {
            tty->canon_buf[tty->canon_len++] = c;
            tty_echo(tty, c);
        }

        // On newline, transfer canonical buffer to input buffer
        if (c == '\n') {
            for (uint i = 0; i < tty->canon_len; i++) {
                tty_add_to_buffer(tty, tty->canon_buf[i]);
            }
            tty->canon_len = 0;
            tty->canon_ready = 1;
        }
    } else {
        // Raw mode - add directly to buffer
        tty_add_to_buffer(tty, c);
        tty_echo(tty, c);
    }
}

// Read from TTY
int tty_read(struct tty_struct *tty, char *buf, size_t count) {
    if (!tty || !buf || count == 0)
        return -1;

    // In canonical mode, wait for line to be ready
    if (tty->lflags & L_ICANON) {
        // Simple busy-wait (proper implementation would block)
        while (!tty->canon_ready && tty->input_count == 0) {
            // In a real OS, we'd sleep here
            // For now, just check for any available input
            asm volatile("hlt");
        }
        tty->canon_ready = 0;
    }

    // Read from input buffer
    size_t read = 0;
    while (read < count && tty->input_count > 0) {
        buf[read++] = tty->input_buf[tty->input_tail];
        tty->input_tail = (tty->input_tail + 1) % TTY_INPUT_SIZE;
        tty->input_count--;

        // In canonical mode, stop at newline
        if ((tty->lflags & L_ICANON) && buf[read - 1] == '\n') {
            break;
        }
    }

    return read;
}

// Write to TTY
int tty_write(struct tty_struct *tty, const char *buf, size_t count) {
    if (!tty || !buf)
        return -1;

    for (size_t i = 0; i < count; i++) {
        tty_output_char(tty, buf[i]);
    }

    return count;
}

void tty_flush_input(struct tty_struct *tty) {
    if (!tty) return;

    tty->input_head = 0;
    tty->input_tail = 0;
    tty->input_count = 0;
    tty->canon_len = 0;
    tty->canon_ready = 0;
}

// Keyboard input handler - routes to active TTY
static void tty_input_handler(struct key_event *ev, void *ctx) {
    (void)ctx;

    // Only handle key presses
    if (!ev->pressed)
        return;

    // Skip VT switch keys (Alt+Fn)
    if ((ev->flags & MOD_ALT) &&
        ev->keycode >= KEY_F1 && ev->keycode <= KEY_F8) {
        return;
    }

    // Skip scroll keys (Shift+PgUp/PgDn)
    if ((ev->flags & MOD_SHIFT) &&
        (ev->keycode == KEY_PGUP || ev->keycode == KEY_PGDN ||
         ev->keycode == KEY_UP || ev->keycode == KEY_DOWN ||
         ev->keycode == KEY_HOME || ev->keycode == KEY_END)) {
        return;
    }

    // Get active TTY
    struct tty_struct *tty = tty_current();
    if (!tty) return;

    // Handle backspace/delete keys
    if (ev->keycode == KEY_BACKSPACE) {
        tty_input_char(tty, C_ERASE);
        return;
    }

    // Handle enter key
    if (ev->keycode == KEY_ENTER) {
        tty_input_char(tty, '\n');
        return;
    }

    // Handle ASCII characters
    if (ev->ascii) {
        tty_input_char(tty, ev->ascii);
    }
}
