#include "camera.h"
#include "window.h"
#include "log.h"

static constexpr v3 world_up = {0,1,0};

camera_t::camera_t()
{
    set_perspective(radians(90.0f), 1.0f);
    position = vec3(0);
    pitch    = 0;
    yaw      = RADIANS_90DEG;
}

camera_t::camera_t(v3 pos, v3 look_at)
{
    set_perspective(radians(90.0f), 1.0f);
    position = pos;
    lookat(look_at);
}

void camera_t::lookat(v3 pos)
{
    v3 dir = pos - position;
    yaw    = atan2f(dir.z, dir.x);
    pitch  = atan2f(dir.y, sqrtf(dir.z*dir.z + dir.x*dir.x));
}

void camera_t::set_perspective(f32 fov, f32 aspect)
{
    is_ortho = false;
    this->fov = fov;
    m_proj = perspective(fov, aspect, znear, zfar);
}

void camera_t::set_orthographic(f32 width, f32 aspect)
{
    is_ortho = true;
    f32 u = width;
    f32 v = width * aspect;
    m_proj = orthographic(-u, u, -v, v, znear, zfar);
}

void camera_t::update(f32 dt, window_t& window)
{
    if (is_frozen) return;
    v2 offset = get_mouse_move_direction(window) * sensitivity;
    // looking around
    if (is_button_down(window, MOUSE_BUTTON_1) 
            && !is_button_down(window, MOUSE_BUTTON_2))
    {
        pitch += offset.y;
        yaw   += offset.x;
        pitch = clamp(pitch, -RADIANS_90DEG, RADIANS_90DEG);
    }

    v3 direction;
    direction.x = cosf(yaw) * cosf(pitch);
    direction.y = sinf(pitch);
    direction.z = sinf(yaw) * cosf(pitch);

    v3 fwd   = normalize(direction);
    v3 right = normalize(cross(fwd, world_up));
    v3 up    = normalize(cross(right, fwd));

    // panning
    if (is_button_down(window, MOUSE_BUTTON_2))
    {
        position += (up * offset.y + right * offset.x) * 3.0f;
    }

    const f32 dv = dt * move_speed;
    if (is_key_down(window, KEY_W))
    {
        position += fwd * dv;
    }
    if (is_key_down(window, KEY_S))
    {
        position -= fwd * dv;
    }
    if (is_key_down(window, KEY_A))
    {
        position -= right * dv;
    }
    if (is_key_down(window, KEY_D))
    {
        position += right * dv;
    }

    m_view = view_matrix(position, fwd, up, right);
}

