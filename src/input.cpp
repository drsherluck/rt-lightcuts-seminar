#include "input.h"
#include "window.h"
#include "log.h"

#include <X11/keysym.h>
#include <stdlib.h>

void input_manager_t::init(xcb_connection_t* con)
{
	struct key_mapping_t {
		u32 key;
		u32 keysym;		
	};

	key_mapping_t key_mapping[] = {
		{ KEY_W, XK_w },
		{ KEY_A, XK_a },
		{ KEY_S, XK_s },
		{ KEY_D, XK_d },
		{ KEY_ESC, XK_Escape },
		{ KEY_P, XK_p },
		{ KEY_0, XK_0 },
		{ KEY_ARROW_UP, XK_Up },
		{ KEY_ARROW_DOWN, XK_Down },
		{ KEY_ARROW_LEFT, XK_Left },
		{ KEY_ARROW_RIGHT, XK_Right },
		{ KEY_SHIFT_L, XK_Shift_L },
		{ KEY_SHIFT_R, XK_Shift_R },
		{ KEY_R, XK_r },
		{ KEY_PLUS, XK_plus },
		{ KEY_MINUS, XK_minus },
		{ KEY_Q, XK_q },
	};
	size_t keys_count = sizeof(key_mapping) / sizeof(key_mapping_t);

	const xcb_setup_t *setup = xcb_get_setup(con);
	xcb_get_keyboard_mapping_cookie_t map_cookie;
	map_cookie = xcb_get_keyboard_mapping(con, setup->min_keycode, setup->max_keycode - setup->min_keycode + 1);

	xcb_generic_error_t* err = nullptr;
	xcb_get_keyboard_mapping_reply_t* mapping;
	mapping = xcb_get_keyboard_mapping_reply(con, map_cookie, &err);
	if (err)
	{
		LOG_ERROR("Failed retreive key mappings");
	}

	i32 keycodes_count = mapping->length / mapping->keysyms_per_keycode;
	xcb_keysym_t* keysyms = (xcb_keysym_t*)(mapping + 1);

	for (i32 i = 0; i < keycodes_count; ++i)
	{
		xcb_keycode_t keycode = setup->min_keycode + i;
		for (i32 j = 0; j < mapping->keysyms_per_keycode; ++j)
		{
			xcb_keysym_t keysym = keysyms[j + i * mapping->keysyms_per_keycode];
			// map keycode to key
			for (size_t k = 0; k < keys_count; ++k)
			{
				if (key_mapping[k].keysym == keysym)
				{
					keycode_map[keycode] = key_mapping[k].key;
                    keyboard[key_mapping[k].key] = UNKOWN;
                    LOG_INFO("key %d mapped to %d", key_mapping[k].key, keycode);
					break;
				}
			}
		}
	}

    for (auto& button : mouse)
    {
        button = UNKOWN;
    }

	prev_mouse_pos = curr_mouse_pos = vec2(-1.0f); // center of screen
	pointer_inside_window = true;
	free(mapping);
}

void input_manager_t::update()
{
    for (auto& key : keyboard) {
        if (key & RELEASED)
        {
            key = UNKOWN;
            continue;
        }
        if (key & PRESSED)
        {
            key = key_state_t(key & ~PRESSED);
        }
    }
    for (auto& button : mouse)
    {
        if (button & RELEASED)
        {
            button = UNKOWN;
            continue;
        }
        if (button & PRESSED)
        {
            button = key_state_t(button & ~PRESSED);
        }
    }

    prev_mouse_pos = curr_mouse_pos;
}
    
void input_manager_t::handle_event(window_t* window, xcb_generic_event_t *event, xcb_generic_event_t *next)
{
	switch (event->response_type & ~0x80)
	{
		//https://www.x.org/releases/X11R7.7/doc/libxcb/tutorial/index.html#userinput
		case XCB_BUTTON_PRESS: 
			{
				auto ev = (const xcb_button_press_event_t*)event;
				if (ev->detail == 1)
				{
                    mouse[MOUSE_BUTTON_1] = PRESSED | DOWN;
				}
				if (ev->detail == 3)
				{
                    mouse[MOUSE_BUTTON_2] = PRESSED | DOWN;
				}
				break;
			}
		case XCB_BUTTON_RELEASE:
			{
				auto ev = (const xcb_button_release_event_t*)event;
				if (ev->detail == 1) 
				{
                    mouse[MOUSE_BUTTON_1] |= RELEASED;
				}
				if (ev->detail == 3)
				{
                    mouse[MOUSE_BUTTON_2] |= RELEASED;
				}
				break;
			}
		case XCB_KEY_PRESS:
			{
				auto ev = (const xcb_key_press_event_t*)event;
				if (keycode_map.find(ev->detail) != keycode_map.end())
				{
					u32 key = keycode_map[ev->detail];
                    if (!(keyboard[key] & DOWN))
                    {
                        keyboard[key] = PRESSED | DOWN;
                    }
				}
				break;
			}
		case XCB_KEY_RELEASE:
			{
				auto ev = (const xcb_key_release_event_t*)event;
				if (next && (next->response_type & ~0x80) == XCB_KEY_PRESS) 
				{
					auto pr = (const xcb_key_press_event_t*)next;
					if (pr->detail == ev->detail && pr->time == ev->time)
                        break; // skip key repeat
				}
				if (keycode_map.find(ev->detail) != keycode_map.end())
				{
					u32 key = keycode_map[ev->detail];
                    keyboard[key] |= RELEASED;
				}
				break;
			}
		case XCB_MOTION_NOTIFY: 
			{
				auto ev = (const xcb_enter_notify_event_t*)event;
				if (pointer_inside_window)
				{
                    curr_mouse_pos.x = ev->event_x/static_cast<f32>(window->width);
                    curr_mouse_pos.y = ev->event_y/static_cast<f32>(window->height);
				}
				break;
			}
		case XCB_ENTER_NOTIFY:
			{
				pointer_inside_window = true;	
				break;
			}
		case XCB_LEAVE_NOTIFY:
			{
				pointer_inside_window = false;
				curr_mouse_pos = prev_mouse_pos = vec2(-1.0f);
				break;
			}
		default: break;
	}
}


