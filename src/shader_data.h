#ifndef SHADER_DATA_H
#define SHADER_DATA_H

struct light_t
{
    vec3 pos;
    vec3 color;
};

struct encoded_t
{
    uint code;
    uint id;
};

struct node_t
{
    vec3 bbox_min;
    float intensity;
    vec3 bbox_max;
    uint id;
};

#endif // SHADER_DATA_H
