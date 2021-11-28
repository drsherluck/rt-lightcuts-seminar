#define GLFW_INCLUDE_VULKAN
#include "window.h"
#include "log.h"

window_t::window_t()
{
    if (!glfwInit())
    {
        LOG_FATAL("Failed to initialize glfw");
    }
	running = true;
}


void window_t::set_title(std::string name)
{
    glfwSetWindowTitle(window, name.c_str());
}

void window_t::create_window(std::string name, u16 width, u16 height)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_NO_API);
    window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
    input_manager.init(this);
	this->width = width;
	this->height = height;
}

bool window_t::poll_events()
{
    input_manager.update();
    glfwPollEvents();
    running = !glfwWindowShouldClose(window);
	return running;
}

std::vector<const char*> window_t::get_required_vulkan_extensions()
{
    u32 count;
    const char** _exts = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> extensions(_exts, _exts + count);
	return extensions;
}

// creates a vulkan surface for this window.
VkResult window_t::create_surface(VkInstance instance, VkSurfaceKHR *surface)
{
	return glfwCreateWindowSurface(instance, window, nullptr, surface);
}

u16 window_t::get_width() const
{
	return width;
}

u16 window_t::get_height() const
{
	return height;
}

window_t::~window_t()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}
