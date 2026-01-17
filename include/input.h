#pragma once
#include <types.h>
#include <kb.h>

// Maximum number of input handlers
#define INPUT_MAX_HANDLERS 16

// Input handler callback type
typedef void (*input_handler_t)(struct key_event *event, void *ctx);

// Input handler registration
// keycode_filter: 0 = receive all keys, or specific KEY_* to filter
// Returns handler ID (>= 0) on success, -1 on failure
int input_register(input_handler_t cb, void *ctx, uint8_t keycode_filter);

// Unregister a handler by ID
void input_unregister(int handler_id);

// Dispatch key event to all registered handlers
// Called by kb_handler
void input_dispatch(struct key_event *event);

// Initialize input system
void input_init(void);
