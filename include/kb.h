#pragma once
#include <types.h>

// PS/2 Keyboard ports
#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64
#define KB_CMD_PORT     0x64

// Modifier flags
#define MOD_SHIFT   0x01
#define MOD_CTRL    0x02
#define MOD_ALT     0x04
#define MOD_CAPS    0x08

// Special key codes (for non-ASCII keys)
// Use values >= 0x80 to avoid collision with scancodes
#define KEY_ESC         0x01
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_LALT        0x38
#define KEY_SPACE       0x39
#define KEY_CAPS        0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44
#define KEY_F11         0x57
#define KEY_F12         0x58

// Extended keys (E0 prefixed) - mapped to high values
#define KEY_UP          0x80
#define KEY_DOWN        0x81
#define KEY_LEFT        0x82
#define KEY_RIGHT       0x83
#define KEY_PGUP        0x84
#define KEY_PGDN        0x85
#define KEY_HOME        0x86
#define KEY_END         0x87
#define KEY_INSERT      0x88
#define KEY_DELETE      0x89
#define KEY_RCTRL       0x8A
#define KEY_RALT        0x8B

// Key event - universal format for all key input
struct key_event {
    uint8_t scancode;   // Raw PS/2 scancode
    uint8_t keycode;    // Normalized key code (KEY_A, KEY_UP, etc.)
    char ascii;         // ASCII char (0 if non-printable)
    uint8_t flags;      // MOD_SHIFT | MOD_CTRL | MOD_ALT | MOD_CAPS
    uint8_t pressed;    // 1 = pressed, 0 = released
};

// Modifier state tracking
struct kb_state {
    uint8_t shift   : 1;
    uint8_t ctrl    : 1;
    uint8_t alt     : 1;
    uint8_t caps    : 1;
    uint8_t extended: 1;  // E0 prefix received
};

// Circular event buffer
#define KB_BUFFER_SIZE 64

struct kb_buffer {
    struct key_event events[KB_BUFFER_SIZE];
    uint head;
    uint tail;
};

// Forward declaration for trapframe
struct trapframe;

// Functions
void kb_init(void);                           // Initialize keyboard, unmask IRQ1
void kb_handler(struct trapframe *tf);        // IRQ1 interrupt handler
int kb_get_event(struct key_event *ev);       // Get event (non-blocking), returns 1 if event available
int kb_poll(void);                            // Check if key event available
char kb_getchar(void);                        // Blocking character read

// Access to current modifier state
uint8_t kb_get_modifiers(void);
