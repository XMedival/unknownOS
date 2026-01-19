#pragma once
#include <types.h>

// Forward declaration
struct vt_struct;

// Number of TTYs (matches number of VTs)
#define NR_TTYS         8

// Input buffer size
#define TTY_INPUT_SIZE  256

// Line discipline flags
#define L_ECHO      0x01    // Echo input characters
#define L_ICANON    0x02    // Canonical mode (line-buffered)
#define L_ISIG      0x04    // Enable signals (Ctrl+C, etc.)
#define L_ICRNL     0x08    // Map CR to NL on input

// Control characters
#define CTRL(c)     ((c) & 0x1F)
#define C_INTR      CTRL('C')   // Interrupt
#define C_QUIT      CTRL('\\')  // Quit
#define C_ERASE     0x7F        // Backspace/Delete
#define C_KILL      CTRL('U')   // Kill line
#define C_EOF       CTRL('D')   // End of file
#define C_SUSP      CTRL('Z')   // Suspend

// TTY structure
struct tty_struct {
    int index;                      // TTY number (0-7)

    // Associated VT
    struct vt_struct *vt;

    // Line discipline flags
    uint32_t lflags;                // L_ECHO | L_ICANON | ...

    // Input buffer (ring buffer)
    char input_buf[TTY_INPUT_SIZE];
    uint input_head;                // Write position
    uint input_tail;                // Read position
    uint input_count;               // Characters in buffer

    // Canonical mode line buffer
    char canon_buf[TTY_INPUT_SIZE];
    uint canon_len;                 // Characters in canonical buffer
    int canon_ready;                // Line ready for reading

    // Reference count
    int open_count;
};

// Global TTY array
extern struct tty_struct ttys[NR_TTYS];

// TTY API
void tty_init(void);
struct tty_struct *tty_get(int index);
int tty_open(struct tty_struct *tty);
int tty_close(struct tty_struct *tty);
int tty_read(struct tty_struct *tty, char *buf, size_t count);
int tty_write(struct tty_struct *tty, const char *buf, size_t count);
void tty_input_char(struct tty_struct *tty, char c);
void tty_flush_input(struct tty_struct *tty);

// Get TTY for active VT
struct tty_struct *tty_current(void);
