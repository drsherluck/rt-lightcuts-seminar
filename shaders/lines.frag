#version 460

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform constants 
{
    vec3 color;
};

void main()
{
    out_color = vec4(color, 1.0);
}
