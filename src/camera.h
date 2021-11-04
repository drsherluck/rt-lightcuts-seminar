#ifndef CAMERA_H
#define CAMERA_H

#include "math.h"

#define RADIANS_90DEG 1.570796

struct  window_t;

struct camera_t
{
    v3 position;
	f32 fov;
	f32 pitch;
	f32 yaw;

	m4x4 m_proj;
	m4x4 m_view;

	f32 move_speed = 5.0f;
    f32 sensitivity = 2.0f;

	camera_t();
    camera_t(v3 position, v3 look_at);
	void set_fov(f32 fov, f32 aspect);
	void update(f32 dt, window_t& window);
    void lookat(v3 pos);

	v3 get_direction() const
	{
		return m_view[2].xyz;
	}

	m4x4 get_matrix() const
	{
		return m_proj * m_view;
	}
};

#endif // CAMERA_H
