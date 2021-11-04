#ifndef INPUT_H
#define INPUT_H

#include "math.h"

#include <xcb/xcb.h>
#include <set>
#include <unordered_map>

#define KEY_W   0
#define KEY_A   1
#define KEY_S   2
#define KEY_D   3
#define KEY_ESC 4
#define KEY_P   5

#define MOUSE_BUTTON_1 0
#define MOUSE_BUTTON_2 1

// keystates
#define UNKOWN   0u
#define PRESSED  1u
#define RELEASED 2u
#define DOWN     4u

typedef u8 key_state_t;

struct window_t;

struct input_manager_t
{
	std::unordered_map<xcb_keycode_t, u32> keycode_map;
    key_state_t keyboard[6];
    key_state_t mouse[2];
	v2 prev_mouse_pos;
	v2 curr_mouse_pos;
	bool pointer_inside_window;

	void init(xcb_connection_t* con);
    void update(); // update states
	void handle_event(window_t* window, xcb_generic_event_t *event, xcb_generic_event_t *prev);
};

#endif // INPUT_H

