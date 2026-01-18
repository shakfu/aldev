/* loki_event.c - Abstract input event layer implementation
 *
 * Implements the event abstraction layer defined in event.h:
 * - Keycode to event conversion
 * - Terminal and test event sources
 * - Event construction helpers
 */

#include "event.h"
#include "internal.h"
#include "terminal.h"
#include <stdlib.h>
#include <string.h>

/* ======================= Keycode Conversion =============================== */

/**
 * Decompose a legacy keycode into base keycode and modifiers.
 * Handles shift-modified keycodes (SHIFT_ARROW_UP -> ARROW_UP + MOD_SHIFT).
 */
EditorEvent event_from_keycode(int keycode) {
    EditorEvent ev = {0};
    ev.type = EVENT_KEY;
    ev.data.key.modifiers = MOD_NONE;

    /* Handle shift-modified arrow keys */
    switch (keycode) {
        case SHIFT_ARROW_UP:
            ev.data.key.keycode = ARROW_UP;
            ev.data.key.modifiers = MOD_SHIFT;
            break;
        case SHIFT_ARROW_DOWN:
            ev.data.key.keycode = ARROW_DOWN;
            ev.data.key.modifiers = MOD_SHIFT;
            break;
        case SHIFT_ARROW_LEFT:
            ev.data.key.keycode = ARROW_LEFT;
            ev.data.key.modifiers = MOD_SHIFT;
            break;
        case SHIFT_ARROW_RIGHT:
            ev.data.key.keycode = ARROW_RIGHT;
            ev.data.key.modifiers = MOD_SHIFT;
            break;
        case SHIFT_RETURN:
            ev.data.key.keycode = ENTER;
            ev.data.key.modifiers = MOD_SHIFT;
            break;

        /* Ctrl key combinations (1-26 except special cases) */
        case CTRL_C:
        case CTRL_D:
        case CTRL_E:
        case CTRL_F:
        case CTRL_G:
        case CTRL_H:
        case CTRL_L:
        case CTRL_P:
        case CTRL_Q:
        case CTRL_S:
        case CTRL_T:
        case CTRL_U:
        case CTRL_W:
        case CTRL_X:
            ev.data.key.keycode = keycode + 'a' - 1;  /* Convert to letter */
            ev.data.key.modifiers = MOD_CTRL;
            break;

        /* Special case: Tab (Ctrl-I) and Enter (Ctrl-M) are not ctrl combos */
        case TAB:
        case ENTER:
        case ESC:
        case BACKSPACE:
            ev.data.key.keycode = keycode;
            break;

        default:
            ev.data.key.keycode = keycode;
            break;
    }

    /* Populate UTF-8 for printable ASCII */
    if (ev.data.key.keycode >= 32 && ev.data.key.keycode < 127 &&
        !(ev.data.key.modifiers & (MOD_CTRL | MOD_ALT))) {
        ev.data.key.utf8[0] = (char)ev.data.key.keycode;
        ev.data.key.utf8[1] = '\0';
        ev.data.key.utf8_len = 1;
    } else {
        ev.data.key.utf8[0] = '\0';
        ev.data.key.utf8_len = 0;
    }

    return ev;
}

/**
 * Recompose event to legacy keycode.
 */
int event_to_keycode(const EditorEvent *ev) {
    if (!ev || ev->type != EVENT_KEY) {
        return 0;
    }

    int keycode = ev->data.key.keycode;
    uint8_t mods = ev->data.key.modifiers;

    /* Handle Ctrl modifier */
    if (mods & MOD_CTRL) {
        /* Check if it's a letter that maps to a control code */
        if (keycode >= 'a' && keycode <= 'z') {
            return keycode - 'a' + 1;
        }
        /* Control codes that don't map cleanly stay as-is */
    }

    /* Handle Shift modifier for arrow keys */
    if (mods & MOD_SHIFT) {
        switch (keycode) {
            case ARROW_UP:    return SHIFT_ARROW_UP;
            case ARROW_DOWN:  return SHIFT_ARROW_DOWN;
            case ARROW_LEFT:  return SHIFT_ARROW_LEFT;
            case ARROW_RIGHT: return SHIFT_ARROW_RIGHT;
            case ENTER:       return SHIFT_RETURN;
        }
    }

    return keycode;
}

/* ======================= Event Construction =============================== */

EditorEvent event_key(int keycode, uint8_t modifiers) {
    EditorEvent ev = {0};
    ev.type = EVENT_KEY;
    ev.data.key.keycode = keycode;
    ev.data.key.modifiers = modifiers;

    /* Populate UTF-8 for printable ASCII */
    if (keycode >= 32 && keycode < 127 && !(modifiers & (MOD_CTRL | MOD_ALT))) {
        ev.data.key.utf8[0] = (char)keycode;
        ev.data.key.utf8[1] = '\0';
        ev.data.key.utf8_len = 1;
    } else {
        ev.data.key.utf8[0] = '\0';
        ev.data.key.utf8_len = 0;
    }

    return ev;
}

EditorEvent event_command(const char *command) {
    EditorEvent ev = {0};
    ev.type = EVENT_COMMAND;
    ev.data.command.command = command;
    return ev;
}

EditorEvent event_action(const char *action, const char *args) {
    EditorEvent ev = {0};
    ev.type = EVENT_ACTION;
    ev.data.action.action = action;
    ev.data.action.args = args;
    return ev;
}

EditorEvent event_resize(int rows, int cols) {
    EditorEvent ev = {0};
    ev.type = EVENT_RESIZE;
    ev.data.resize.rows = rows;
    ev.data.resize.cols = cols;
    return ev;
}

EditorEvent event_quit(void) {
    EditorEvent ev = {0};
    ev.type = EVENT_QUIT;
    return ev;
}

/* ======================= Terminal Event Source ============================ */

typedef struct {
    int fd;
} TerminalSourceData;

static EditorEvent terminal_source_read(EventSource *src, int timeout_ms) {
    TerminalSourceData *data = (TerminalSourceData *)src->data;
    (void)timeout_ms;  /* terminal_read_key has its own timeout */

    int keycode = terminal_read_key(data->fd);
    return event_from_keycode(keycode);
}

static int terminal_source_poll(EventSource *src) {
    (void)src;
    /* Not implemented - would need select/poll on fd */
    return 1;  /* Always ready (blocking read) */
}

static void terminal_source_destroy(EventSource *src) {
    if (src) {
        free(src->data);
        free(src);
    }
}

EventSource *event_source_terminal(int fd) {
    EventSource *src = calloc(1, sizeof(EventSource));
    if (!src) return NULL;

    TerminalSourceData *data = calloc(1, sizeof(TerminalSourceData));
    if (!data) {
        free(src);
        return NULL;
    }

    data->fd = fd;
    src->data = data;
    src->read = terminal_source_read;
    src->poll = terminal_source_poll;
    src->destroy = terminal_source_destroy;

    return src;
}

/* ======================= Test Event Source ================================ */

#define TEST_EVENT_QUEUE_SIZE 64

typedef struct {
    EditorEvent queue[TEST_EVENT_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} TestSourceData;

static EditorEvent test_source_read(EventSource *src, int timeout_ms) {
    TestSourceData *data = (TestSourceData *)src->data;
    (void)timeout_ms;

    if (data->count == 0) {
        EditorEvent ev = {0};
        ev.type = EVENT_NONE;
        return ev;
    }

    EditorEvent ev = data->queue[data->head];
    data->head = (data->head + 1) % TEST_EVENT_QUEUE_SIZE;
    data->count--;

    return ev;
}

static int test_source_poll(EventSource *src) {
    TestSourceData *data = (TestSourceData *)src->data;
    return data->count > 0;
}

static void test_source_destroy(EventSource *src) {
    if (src) {
        free(src->data);
        free(src);
    }
}

EventSource *event_source_test(void) {
    EventSource *src = calloc(1, sizeof(EventSource));
    if (!src) return NULL;

    TestSourceData *data = calloc(1, sizeof(TestSourceData));
    if (!data) {
        free(src);
        return NULL;
    }

    src->data = data;
    src->read = test_source_read;
    src->poll = test_source_poll;
    src->destroy = test_source_destroy;

    return src;
}

int event_source_test_push(EventSource *src, const EditorEvent *ev) {
    if (!src || !ev || src->read != test_source_read) {
        return -1;
    }

    TestSourceData *data = (TestSourceData *)src->data;
    if (data->count >= TEST_EVENT_QUEUE_SIZE) {
        return -1;  /* Queue full */
    }

    data->queue[data->tail] = *ev;
    data->tail = (data->tail + 1) % TEST_EVENT_QUEUE_SIZE;
    data->count++;

    return 0;
}

int event_source_test_push_key(EventSource *src, int keycode) {
    EditorEvent ev = event_from_keycode(keycode);
    return event_source_test_push(src, &ev);
}
