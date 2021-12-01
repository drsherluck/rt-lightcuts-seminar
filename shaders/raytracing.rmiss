#version 460 
#extension GL_EXT_ray_tracing : require

struct payload_t
{
    int sample_id;
    float seed;
    vec4 color;
    bool hit;
};
layout(location = 0) rayPayloadInEXT payload_t payload;

void main()
{
    payload.color = vec4(0.0, 0.0, 0.0, 1.0);
    payload.hit = false;
}

