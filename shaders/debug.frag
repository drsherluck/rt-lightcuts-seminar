#version 460

layout(set = 0, binding = 0) uniform sampler2D image;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform constants
{
    float znear;
    float zfar;
};

void main()
{
	float z = texture(image, uv).r;
    //z = znear * zfar / (zfar + z * ( znear - zfar));
    z = 2.0 * znear / (zfar + znear - z * (zfar - znear));
    out_color = vec4(z, z, z, 1.0);
}
