#include <kb.h>
#include <x86.h>
#include <serial.h>
#include <log.h>

// Forward declaration for input dispatch
extern void input_dispatch(struct key_event *event);

// Global keyboard state
static struct kb_state kb_state;
static struct kb_buffer kb_buf;

// US keyboard layout - scancode to ASCII (normal)
static const char scancode_to_ascii[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6',  // 0x00-0x07
    '7', '8', '9', '0', '-', '=', '\b', '\t', // 0x08-0x0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',   // 0x10-0x17
    'o', 'p', '[', ']', '\n', 0,   'a', 's',  // 0x18-0x1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   // 0x20-0x27
    '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',  // 0x28-0x2F
    'b', 'n', 'm', ',', '.', '/', 0,   '*',   // 0x30-0x37
    0,   ' ', 0,   0,   0,   0,   0,   0,     // 0x38-0x3F
    0,   0,   0,   0,   0,   0,   0,   '7',   // 0x40-0x47 (numpad)
    '8', '9', '-', '4', '5', '6', '+', '1',   // 0x48-0x4F
    '2', '3', '0', '.', 0,   0,   0,   0,     // 0x50-0x57
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x58-0x5F
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x60-0x67
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x68-0x6F
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x70-0x77
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x78-0x7F
};

// US keyboard layout - scancode to ASCII (shifted)
static const char scancode_to_ascii_shift[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^',  // 0x00-0x07
    '&', '*', '(', ')', '_', '+', '\b', '\t', // 0x08-0x0F
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',   // 0x10-0x17
    'O', 'P', '{', '}', '\n', 0,   'A', 'S',  // 0x18-0x1F
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   // 0x20-0x27
    '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',   // 0x28-0x2F
    'B', 'N', 'M', '<', '>', '?', 0,   '*',   // 0x30-0x37
    0,   ' ', 0,   0,   0,   0,   0,   0,     // 0x38-0x3F
    0,   0,   0,   0,   0,   0,   0,   '7',   // 0x40-0x47 (numpad)
    '8', '9', '-', '4', '5', '6', '+', '1',   // 0x48-0x4F
    '2', '3', '0', '.', 0,   0,   0,   0,     // 0x50-0x57
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x58-0x5F
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x60-0x67
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x68-0x6F
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x70-0x77
    0,   0,   0,   0,   0,   0,   0,   0,     // 0x78-0x7F
};

// Extended scancode (E0 prefix) to keycode mapping
static uint8_t extended_to_keycode(uint8_t scancode) {
    switch (scancode) {
    case 0x48: return KEY_UP;
    case 0x50: return KEY_DOWN;
    case 0x4B: return KEY_LEFT;
    case 0x4D: return KEY_RIGHT;
    case 0x49: return KEY_PGUP;
    case 0x51: return KEY_PGDN;
    case 0x47: return KEY_HOME;
    case 0x4F: return KEY_END;
    case 0x52: return KEY_INSERT;
    case 0x53: return KEY_DELETE;
    case 0x1D: return KEY_RCTRL;
    case 0x38: return KEY_RALT;
    default:   return 0;
    }
}

// Add event to buffer
static void kb_buf_push(struct key_event *ev) {
    uint next = (kb_buf.head + 1) % KB_BUFFER_SIZE;
    if (next != kb_buf.tail) {
        kb_buf.events[kb_buf.head] = *ev;
        kb_buf.head = next;
    }
    // If buffer is full, event is dropped
}

// Get current modifier flags
static uint8_t get_mod_flags(void) {
    uint8_t flags = 0;
    if (kb_state.shift) flags |= MOD_SHIFT;
    if (kb_state.ctrl)  flags |= MOD_CTRL;
    if (kb_state.alt)   flags |= MOD_ALT;
    if (kb_state.caps)  flags |= MOD_CAPS;
    return flags;
}

void kb_init(void) {
    // Clear state
    kb_state.shift = 0;
    kb_state.ctrl = 0;
    kb_state.alt = 0;
    kb_state.caps = 0;
    kb_state.extended = 0;

    // Clear buffer
    kb_buf.head = 0;
    kb_buf.tail = 0;

    // Drain any pending keyboard data
    while (inb(KB_STATUS_PORT) & 0x01) {
        inb(KB_DATA_PORT);
    }
}

void kb_handler(struct trapframe *tf) {
    (void)tf;  // Unused

    uint8_t scancode = inb(KB_DATA_PORT);

    // Handle E0 prefix for extended keys
    if (scancode == 0xE0) {
        kb_state.extended = 1;
        return;
    }

    // Handle E1 prefix (Pause key) - just ignore for now
    if (scancode == 0xE1) {
        return;
    }

    // Determine if key pressed or released
    uint8_t pressed = !(scancode & 0x80);
    uint8_t code = scancode & 0x7F;

    struct key_event ev = {0};
    ev.scancode = scancode;
    ev.pressed = pressed;
    ev.flags = get_mod_flags();

    if (kb_state.extended) {
        kb_state.extended = 0;
        ev.keycode = extended_to_keycode(code);

        // Handle extended modifier keys
        if (ev.keycode == KEY_RCTRL) {
            kb_state.ctrl = pressed;
            ev.flags = get_mod_flags();
        } else if (ev.keycode == KEY_RALT) {
            kb_state.alt = pressed;
            ev.flags = get_mod_flags();
        }
    } else {
        // Regular scancode
        ev.keycode = code;

        // Update modifier state
        switch (code) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            kb_state.shift = pressed;
            ev.flags = get_mod_flags();
            break;
        case KEY_LCTRL:
            kb_state.ctrl = pressed;
            ev.flags = get_mod_flags();
            break;
        case KEY_LALT:
            kb_state.alt = pressed;
            ev.flags = get_mod_flags();
            break;
        case KEY_CAPS:
            if (pressed) {
                kb_state.caps = !kb_state.caps;
                ev.flags = get_mod_flags();
            }
            break;
        }

        // Get ASCII value for printable keys (only on press)
        if (pressed && code < 128) {
            int use_shift = kb_state.shift;
            char c = use_shift ? scancode_to_ascii_shift[code] : scancode_to_ascii[code];

            // Handle caps lock for letters
            if (kb_state.caps && c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            } else if (kb_state.caps && c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }

            ev.ascii = c;
        }
    }

    // Add to buffer
    kb_buf_push(&ev);

    // Dispatch to input system
    input_dispatch(&ev);
}

int kb_get_event(struct key_event *ev) {
    if (kb_buf.tail == kb_buf.head) {
        return 0;  // Buffer empty
    }

    *ev = kb_buf.events[kb_buf.tail];
    kb_buf.tail = (kb_buf.tail + 1) % KB_BUFFER_SIZE;
    return 1;
}

int kb_poll(void) {
    return kb_buf.tail != kb_buf.head;
}

char kb_getchar(void) {
    struct key_event ev;

    while (1) {
        if (kb_get_event(&ev)) {
            if (ev.pressed && ev.ascii) {
                return ev.ascii;
            }
        }
        hlt();  // Wait for interrupt
    }
}

uint8_t kb_get_modifiers(void) {
    return get_mod_flags();
}
