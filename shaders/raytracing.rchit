#version 460 core
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : enable

struct vertex_t
{
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

struct light_t
{
    vec3 position;
    vec3 color;
};

struct material
{
    vec3 base_color;
    float roughness;
    float metalness;
    float emissive;
};

struct mesh_info
{
    int material_index;
    uint index_offset;
    uint vertex_offset;
};

struct model_data
{
    int mesh_index;
    mat4 model;
    mat4 normal;
};

layout(set = 0, binding = 1) readonly buffer lights_buffer
{
    light_t lights[]; 
};

layout(set = 0, binding = 2) readonly buffer model_sbo
{
    model_data models[];
};

layout(set = 0, binding = 3) readonly buffer material_sbo
{
    material materials[];
};

layout(set = 0, binding = 4) readonly buffer mesh_buffer
{
    mesh_info meshes[];
};

layout(buffer_reference, scalar) readonly buffer vertex_buffer 
{
    vertex_t vertices[];
};

layout(buffer_reference, scalar) readonly buffer index_buffer 
{
    uint indices[];
};

layout(set = 1, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 1, binding = 1) uniform scene_buffer
{
    uint64_t vertex_address;
    uint64_t index_address;
} scene;

layout(push_constant) uniform constants
{
    int num_lights;
};


layout(location = 0) rayPayloadInEXT vec4 payload;
layout(location = 1) rayPayloadEXT bool is_shadow;
hitAttributeEXT vec2 attribs;

void main()
{
    vertex_buffer vbo = vertex_buffer(scene.vertex_address);
    index_buffer  ibo = index_buffer(scene.index_address);

    model_data md = models[gl_InstanceCustomIndexEXT];
    mesh_info info = meshes[md.mesh_index];
    const uint idx = uint(info.index_offset) + 3 * gl_PrimitiveID;
    uvec3 triangle = uvec3(ibo.indices[idx + 0], ibo.indices[idx + 1], ibo.indices[idx + 2]) + uvec3(info.vertex_offset);
    const vec3 barycenter = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    const vertex_t v0 = vbo.vertices[triangle.x];
    const vertex_t v1 = vbo.vertices[triangle.y];
    const vertex_t v2 = vbo.vertices[triangle.z];

    vec3 position       = v0.pos * barycenter.x + v1.pos * barycenter.y + v2.pos * barycenter.z;
    vec3 world_position = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));
    vec3 normal         = normalize(v0.normal * barycenter.x + v1.normal * barycenter.y + v2.normal * barycenter.z);
    vec3 world_normal   = normalize(vec3(normal * gl_WorldToObjectEXT));
    vec3 geom_normal    = normalize(cross(v1.pos - v2.pos, v2.pos - v0.pos));

    // limit lights to 1 sample for now
    vec3 temp_color = vec3(0);
    for (int i = 0; i < num_lights; ++i)
    {
        light_t light = lights[i];
        vec3 L = light.position - world_position;
        float distance = length(L);
        L /= distance;

        // white color
        vec3 diffuse = vec3(1) * max(dot(world_normal, L), 0.0);

        vec3 specular = vec3(0);
        float attenuation = 1;

        // shadow check
        if (dot(world_normal, L) > 0)
        {
            vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
            uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

            is_shadow = true;
            traceRayEXT(tlas, 
                flags,
                0xFF,
                0,
                0,
                1, // miss index
                origin,
                0.001,
                L,
                distance,
                1 // payload location = 1
            );

            if (is_shadow)
            {
                attenuation = 0;
            } 
            else 
            {
                vec3 H = normalize(L + gl_WorldRayDirectionEXT);
                specular = vec3(0.5) * pow(max(0.0, dot(H, world_normal)), 5.0);
            }
        }
        attenuation *= 1.0 / (distance * distance);
        temp_color += light.color * attenuation * (diffuse + specular);
    }
    payload = vec4(temp_color, 1.0);
}
