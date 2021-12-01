#version 460
#extension GL_GOOGLE_include_directive : enable

#define GLSL
#include "../src/shader_data.h"
#include "common.inc"
#include "brdf.inc"

layout(std140, set = 0, binding = 0) uniform camera_ubo
{
    camera_ubo_t camera;
};
layout(std430, set = 0, binding = 1) readonly buffer lights_sbo
{
    light_t lights[];
};
layout(std430, set = 0, binding = 3) readonly buffer material_sbo 
{
    material_t materials[];
};
layout(std430, set = 0, binding = 4) readonly buffer mesh_sbo
{
    mesh_info_t meshes[];
};

layout(location = 0) in VS_IN 
{
    vec3 world_pos;
    vec3 normal;
    vec2 uv;
    flat int mesh_index;
    flat int light_count;
};

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 N = normal;
	vec3 V = normalize(camera.pos - world_pos);

    out_color = vec4(0);
    for (int i = 0; i < light_count; ++i)
    {
        light_t light = lights[i];
        vec3 L = normalize(light.pos - world_pos);
        vec3 H = normalize(L + V);
        float HdotV = clamp(dot(H, V), 0.0, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float NdotV = clamp(dot(N, V), 0.0, 1.0);
        float HdotL = clamp(dot(H, L), 0.0, 1.0);
        float NdotL = clamp(dot(N, L), 0.0, 1.0);

        mesh_info_t mesh = meshes[mesh_index];
        if (mesh.material_index == -1)
        {
            vec3 kd = vec3(1.0);
            vec3 ks = vec3(0.5);
            vec3 diffuse = max(0.0, NdotL) * kd;
            vec3 specular = ks * pow(NdotH, 5.0);

            // attenuation
            float dis = length(light.pos - world_pos);
            vec3 L_color = light.color * 10.0/ (dis*dis);

            out_color.xyz += L_color * (diffuse + specular);
            continue;
        }

        material_t mat = materials[mesh.material_index];
        vec3 F0 = mix(vec3(0.04), mat.base_color, mat.metalness);
        vec3 r_diffuse = mat.base_color * (1.0 - mat.metalness);
        float a  = mat.roughness * mat.roughness;
        float a2 =  a * a;
        
        // specular
        float D = d_ggx(a2, NdotH);
        float G = g_smith(a, NdotV, NdotL);
        vec3  F = f_schlick(F0, HdotL);
        vec3 cook_torrance = (D*G*F)/(4.0f*NdotV*NdotL);

        // diffuse
        vec3 lambert = r_diffuse / PI;

        // attenuation
        float dis = length(light.pos - world_pos);
        vec3 L_color = light.color * 10.0/ (dis*dis);

        vec3 brdf = max((vec3(1.0) - F) * lambert + cook_torrance, vec3(0));
        vec3 emissive = mat.base_color * mat.emissive;
        out_color.xyz += emissive + brdf * L_color * NdotL;
    }
    out_color.w = 1;
}
