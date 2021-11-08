#version 460

layout(set = 0, binding = 0) uniform sampler2D image;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

void main()
{
    const float gamma = 2.2;
	vec3 hdr = texture(image, uv).rgb;

    vec3 mapped = hdr / (hdr + vec3(1.0));
    mapped = pow(mapped, vec3(1.0/gamma));
    out_color = vec4(mapped, 1.0);
}
