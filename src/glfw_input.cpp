#include "input.h"
#include "window.h"
#include "log.h"

#include <stdlib.h>

void input_manager_t::init(window_t* window)
{
    for (auto& key : keyboard)
    {
        key = UNKOWN;
    }
    for (auto& button : mouse)
    {
        button = UNKOWN;
    }

	prev_mouse_pos = curr_mouse_pos = vec2(-1.0f); // center of screen
	pointer_inside_window = true;
    this->window = window;
}

void input_manager_t::update()
{
    auto* glfw_window = window->window;
    for (i32 key_code = GLFW_KEY_COMMA; key_code < NUM_KEYS; ++key_code)
    {
        bool is_pressed = (glfwGetKey(glfw_window, key_code) == GLFW_PRESS);
        auto& key = keyboard[key_code];
        if (is_pressed)
        {
            key = ((key & PRESSED) || (key & DOWN)) ? DOWN : PRESSED;
        }
        else 
        {
            key = (key & RELEASED) ? UNKOWN : RELEASED;
        }
    }
    for (i32 button_code = GLFW_MOUSE_BUTTON_1;  button_code < NUM_BUTTONS; ++button_code)
    {
        bool is_pressed = (glfwGetMouseButton(glfw_window, button_code) == GLFW_PRESS);
        auto& button = mouse[button_code];
        if (is_pressed)
        {
            button = ((button & PRESSED) || (button & DOWN)) ? DOWN : PRESSED;
        }
        else 
        {
            button = (button & RELEASED) ? UNKOWN : RELEASED;
        }
    }
    
    if (glfwGetWindowAttrib(glfw_window, GLFW_HOVERED))
    {
        pointer_inside_window = true;
        prev_mouse_pos = curr_mouse_pos;
        f64 x, y;
        glfwGetCursorPos(glfw_window, &x, &y);
        curr_mouse_pos.x = x/static_cast<f32>(window->width);
        curr_mouse_pos.y = y/static_cast<f32>(window->height);
    } 
    else 
    {
        curr_mouse_pos = prev_mouse_pos = vec2(-1.0f);
    }
}
    
