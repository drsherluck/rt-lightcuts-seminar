#version 460

struct model_data
{
    int material_index;
    mat4 model;
    mat4 normal; 
};

layout(std140, set = 0, binding = 2) readonly buffer model_sbo 
{
    model_data models[];
};

layout(push_constant) uniform constants
{
    mat4 projection;
    mat4 view;
    int num_lights;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out VS_OUT
{
    vec3 frag_pos;
    vec3 frag_normal;
    vec2 texcoord;
    flat int material_index;
    flat int light_count;
};

void main() 
{
    model_data data = models[gl_InstanceIndex]; 

    mat4 model_view = view * data.model;
    vec4 pos = model_view * vec4(position, 1);
    gl_Position = projection * pos;

	frag_pos = pos.xyz;
	frag_normal = normalize(mat3(data.normal) * normal);
	texcoord = uv;
    material_index = data.material_index;
    light_count = num_lights;
}
