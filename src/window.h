#ifndef WINDOW_H
#define WINDOW_H

#include "common.h"
#include "input.h"

#include <vector>
#include <string>

#ifndef USE_GLFW
// todo add conditional for different platforms
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>
#include <xcb/xcb.h>
#else
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#endif

struct window_t
{
#ifndef USE_GLFW
	xcb_connection_t  *con;
	xcb_screen_t      *screen;
	xcb_window_t      window;
	xcb_atom_t        wm_delete_atom;
#else
    GLFWwindow*       window;
#endif 
	bool              running;
	u16               width;
	u16               height;
    input_manager_t   input_manager;

	window_t();
	~window_t();

#ifndef USE_GLFW
	void setup_delete_protocol();
	void handle_event(xcb_generic_event_t *event);
#endif

	void create_window(std::string name, u16 width, u16 height); 
	void set_title(std::string name);
	bool poll_events();
	void close() 
    {
        running = false;
    }

	std::vector<const char*> get_required_vulkan_extensions();
	VkResult create_surface(VkInstance instance, VkSurfaceKHR *surface);
	u16 get_width() const;
	u16 get_height() const;
    f32 get_aspect_ratio() const 
    {
        return static_cast<f32>(get_height())/static_cast<f32>(get_width());
    }
};

inline v2 get_mouse_move_direction(window_t& window)
{
    // outside window bounds
    if (window.input_manager.prev_mouse_pos.x == -1.0f)
    {
        return vec2(0);
    }

    v2 dir;
    dir.x = window.input_manager.curr_mouse_pos.x - window.input_manager.prev_mouse_pos.x;
    dir.y = window.input_manager.prev_mouse_pos.y - window.input_manager.curr_mouse_pos.y;
    return dir;
}

// keyboard 
inline bool is_key_down(window_t& window, u32 key)
{
    return window.input_manager.keyboard[key] & DOWN;
}

inline bool is_key_pressed(window_t& window, u32 key) 
{
    return window.input_manager.keyboard[key] & PRESSED;
}

inline bool is_key_released(window_t& window, u32 key)
{
    return window.input_manager.keyboard[key] & RELEASED;
}

// mouse buttons 
inline bool is_button_down(window_t& window, u32 button)
{
    return window.input_manager.mouse[button] & DOWN;
}

inline bool is_button_pressed(window_t& window, u32 button)
{
    return window.input_manager.mouse[button] & PRESSED;
}

inline bool is_button_released(window_t& window, u32 button)
{
    return window.input_manager.mouse[button] & RELEASED;
}

#endif // WINDOW_H
