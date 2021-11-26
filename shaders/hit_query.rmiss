#version 460 
#extension GL_EXT_ray_tracing : require

struct payload_t 
{
    vec3 hit_pos;
    bool hit;
};

layout(location = 0) rayPayloadInEXT payload_t payload;

void main()
{
    payload.hit = false;
}

