#ifndef INPUT_H
#define INPUT_H

#include "math.h"

#ifndef USE_GLFW
#include <xcb/xcb.h>
#endif
#include <set>
#include <unordered_map>

#ifndef USE_GLFW

#define NUM_KEYS 22
#define KEY_W           0
#define KEY_A           1
#define KEY_S           2
#define KEY_D           3
#define KEY_ESC         4
#define KEY_P           5
#define KEY_0           6
#define KEY_ARROW_UP    7
#define KEY_ARROW_DOWN  8
#define KEY_ARROW_LEFT  9
#define KEY_ARROW_RIGHT 10
#define KEY_SHIFT_L     11
#define KEY_SHIFT_R     12
#define KEY_R           13
#define KEY_PLUS        14
#define KEY_MINUS       15
#define KEY_Q           16
#define KEY_N           17
#define KEY_1           18
#define KEY_J           19
#define KEY_K           20
#define KEY_2           21

#define NUM_BUTTONS 2
#define MOUSE_BUTTON_1 0
#define MOUSE_BUTTON_2 1

#else
#include <GLFW/glfw3.h>

#define NUM_KEYS GLFW_KEY_LAST + 1
#define KEY_W           GLFW_KEY_W
#define KEY_A           GLFW_KEY_A
#define KEY_S           GLFW_KEY_S
#define KEY_D           GLFW_KEY_D
#define KEY_ESC         GLFW_KEY_ESCAPE
#define KEY_P           GLFW_KEY_P
#define KEY_0           GLFW_KEY_0
#define KEY_ARROW_UP    GLFW_KEY_UP
#define KEY_ARROW_DOWN  GLFW_KEY_DOWN
#define KEY_ARROW_LEFT  GLFW_KEY_LEFT
#define KEY_ARROW_RIGHT GLFW_KEY_RIGHT
#define KEY_SHIFT_L     GLFW_KEY_LEFT_SHIFT
#define KEY_SHIFT_R     GLFW_KET_RIGHT_SHIFT
#define KEY_R           GLFW_KEY_R
#define KEY_EQUAL       GLFW_KEY_EQUAL
#define KEY_MINUS       GLFW_KEY_MINUS
#define KEY_Q           GLFW_KEY_Q
#define KEY_N           GLFW_KEY_N
#define KEY_1           GLFW_KEY_1
#define KEY_J           GLFW_KEY_J
#define KEY_K           GLFW_KEY_K
#define KEY_2           GLFW_KEY_2

#define NUM_BUTTONS GLFW_MOUSE_BUTTON_8 + 1
#define MOUSE_BUTTON_1 GLFW_MOUSE_BUTTON_1
#define MOUSE_BUTTON_2 GLFW_MOUSE_BUTTON_2

#endif

// keystates
#define UNKOWN   0u
#define PRESSED  1u
#define RELEASED 2u
#define DOWN     4u

typedef u8 key_state_t;

struct window_t;

struct input_manager_t
{
#ifndef USE_GLFW
	std::unordered_map<xcb_keycode_t, u32> keycode_map;
#endif

    key_state_t keyboard[NUM_KEYS];
    key_state_t mouse[NUM_BUTTONS];
	v2 prev_mouse_pos;
	v2 curr_mouse_pos;
	bool pointer_inside_window;

#ifndef USE_GLFW
	void init(xcb_connection_t* con);
	void handle_event(window_t* window, xcb_generic_event_t *event, xcb_generic_event_t *prev);
#else
    // only handle one window
    window_t* window;
    void init(window_t* window);
#endif

    void update(); 
};

#endif // INPUT_H

