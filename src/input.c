#include <input.h>
#include <kb.h>

// Handler entry
struct input_handler {
    input_handler_t callback;
    void *ctx;
    uint8_t keycode_filter;  // 0 = all keys
    uint8_t active;
};

// Registered handlers
static struct input_handler handlers[INPUT_MAX_HANDLERS];

void input_init(void) {
    for (int i = 0; i < INPUT_MAX_HANDLERS; i++) {
        handlers[i].active = 0;
        handlers[i].callback = NULL;
        handlers[i].ctx = NULL;
        handlers[i].keycode_filter = 0;
    }
}

int input_register(input_handler_t cb, void *ctx, uint8_t keycode_filter) {
    if (!cb) return -1;

    for (int i = 0; i < INPUT_MAX_HANDLERS; i++) {
        if (!handlers[i].active) {
            handlers[i].callback = cb;
            handlers[i].ctx = ctx;
            handlers[i].keycode_filter = keycode_filter;
            handlers[i].active = 1;
            return i;
        }
    }

    return -1;  // No free slots
}

void input_unregister(int handler_id) {
    if (handler_id >= 0 && handler_id < INPUT_MAX_HANDLERS) {
        handlers[handler_id].active = 0;
        handlers[handler_id].callback = NULL;
    }
}

void input_dispatch(struct key_event *event) {
    if (!event) return;

    for (int i = 0; i < INPUT_MAX_HANDLERS; i++) {
        if (handlers[i].active && handlers[i].callback) {
            // Check filter
            if (handlers[i].keycode_filter == 0 ||
                handlers[i].keycode_filter == event->keycode) {
                handlers[i].callback(event, handlers[i].ctx);
            }
        }
    }
}
