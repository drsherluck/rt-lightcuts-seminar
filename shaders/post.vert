#version 460

layout(location = 0) out vec2 texcoord;

vec2 positions[4] = vec2[](
    vec2(-1, -1),
    vec2(-1,  1),
    vec2( 1, -1),
    vec2( 1,  1)
);

void main() 
{
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
	texcoord = vec2(pos.x * 0.5 + 0.5, pos.y * 0.5 + 0.5);
}
