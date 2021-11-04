#include "window.h"
#include "log.h"

#include <xcb/xcb_icccm.h>

window_t::window_t()
{
	con = xcb_connect(nullptr, nullptr);

	if (xcb_connection_has_error(con))
	{
		xcb_disconnect(con);
        LOG_FATAL("Failed to connect to xserver");
	}

	const xcb_setup_t *setup = xcb_get_setup(con);
	screen = xcb_setup_roots_iterator(setup).data;
	running = true;
    input_manager.init(con);
}

xcb_intern_atom_cookie_t create_cookie(xcb_connection_t *c, const std::string &s)
{
	return xcb_intern_atom(c, 0, s.size(), s.c_str());
}

xcb_atom_t create_atom(xcb_connection_t *c, xcb_intern_atom_cookie_t cookie)
{
	xcb_atom_t atom = XCB_ATOM_NONE;
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(c, cookie, nullptr);
	if (reply)
	{
		atom = reply->atom;
		free(reply);
	}
	return atom;
}

xcb_atom_t atom(xcb_connection_t *c, const std::string s)
{
	return create_atom(c, create_cookie(c, s));
}

void window_t::set_title(std::string name)
{
	xcb_change_property(con, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, name.size(), name.c_str());
}

void window_t::setup_delete_protocol()
{
	wm_delete_atom = atom(con, "WM_DELETE_WINDOW");
	xcb_atom_t wm_protocols = atom(con, "WM_PROTOCOLS");
	xcb_change_property(con, XCB_PROP_MODE_REPLACE, window, wm_protocols, XCB_ATOM_ATOM, 32, 1, &wm_delete_atom);
}

void window_t::create_window(std::string name, u16 width, u16 height)
{
	u32 mask = XCB_CW_EVENT_MASK;
	u32 values[1];
	values[0] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS
		| XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
		| XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
		| XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW;

	window = xcb_generate_id(con);
	xcb_create_window(
			con,
			XCB_COPY_FROM_PARENT,
			window,
			screen->root,
			0, 0,
			width, height,
			0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT,
			screen->root_visual,
			mask, values );
	xcb_map_window(con, window);
	xcb_flush(con);

	// set no resize hint
	xcb_size_hints_t hints;
	xcb_icccm_size_hints_set_min_size(&hints, width, height);
	xcb_icccm_size_hints_set_max_size(&hints, width, height);
	xcb_icccm_set_wm_size_hints(con, window, XCB_ATOM_WM_NORMAL_HINTS, &hints);

	set_title(name);
	setup_delete_protocol();
	this->width = width;
	this->height = height;
}

void window_t::handle_event(xcb_generic_event_t *event)
{
	switch (event->response_type & ~0x80)
	{
		case XCB_RESIZE_REQUEST:
			{
				auto resize = (const xcb_resize_request_event_t*)event;
				if (resize->width > 0) 
				{
					width = resize->width;
				}
				if (resize->height > 0)
				{
					height = resize->height;
				}
				const static u32 values[] = {width, height};
				xcb_configure_window(con, window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
                LOG_INFO("Resize window");
				break;
			}
		case XCB_CLIENT_MESSAGE:
			{
				auto msg = (const xcb_client_message_event_t*)event;
				if (msg->data.data32[0] == wm_delete_atom)
				{
					running = false;
				}
				break;
			}
		default:
			break;
	}
}

bool window_t::poll_events()
{
	xcb_generic_event_t *prev = nullptr;
	xcb_generic_event_t *curr = xcb_poll_for_event(con);
	xcb_generic_event_t *next = xcb_poll_for_event(con); 
    
    input_manager.update();
	while (curr) 
	{
		handle_event(curr);
		input_manager.handle_event(this, curr, next);
		if (prev)
		{
			free(prev);
		}
		prev = curr;
		curr = next;
		next = xcb_poll_for_event(con);
	}

	free(prev);
	free(curr);
	free(next);
	return running;
}

std::vector<const char*> window_t::get_required_vulkan_extensions()
{
	std::vector<const char*> extensions = {
		VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME 
	};
	return extensions;
}

// creates a vulkan surface for this window.
VkResult window_t::create_surface(VkInstance instance, VkSurfaceKHR *surface)
{
	VkXcbSurfaceCreateInfoKHR info;
	info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	info.flags = 0;
	info.pNext = nullptr;
	info.connection = con;
	info.window = window;

	return vkCreateXcbSurfaceKHR(instance, &info, nullptr, surface);
}

u16 window_t::get_width() const
{
	xcb_get_geometry_cookie_t req = xcb_get_geometry(con, window);
	xcb_get_geometry_reply_t* rep = xcb_get_geometry_reply(con, req, nullptr);
	u16 w = rep->width;
	free(rep);
	return w;
}

u16 window_t::get_height() const
{
	xcb_get_geometry_cookie_t req = xcb_get_geometry(con, window);
	xcb_get_geometry_reply_t* rep = xcb_get_geometry_reply(con, req, nullptr);
	u16 h = rep->height;
	free(rep);
	return h;
}

window_t::~window_t()
{
	xcb_unmap_window(con, window);
	xcb_destroy_window(con, window);
	xcb_disconnect(con);
    LOG_INFO("xcb disconnected from server");
}
