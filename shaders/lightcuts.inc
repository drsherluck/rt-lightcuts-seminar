#define GLSL
#include "../src/shader_data.h"
#include "common.inc"

#define INVALID_ID 0xffffffff
#define FLT_MAX 3.40282e38
#define FLT_MIN 1.17549e-38

// define outside or default to set 0 
#ifndef NODES_SSBO_SET
    #define NODES_SSBO_SET 0
#endif
#ifndef NODES_SSBO_BINDING
    #define NODES_SSBO_BINDING 0
#endif

#define MAX_CUT_SIZE 32
// todo: make push constant
#ifndef USER_MAX_CUT
    #define USER_MAX_CUT 1
#endif

// (hack) define the buffer here so that we can reuse
layout(std430, set = NODES_SSBO_SET, binding = NODES_SSBO_BINDING) readonly buffer nodes_sbo
{
    node_t nodes[];
};

struct light_cut_t
{
    uint id;
    float error;
};

struct selected_light_t
{
    uint id; 
    float prob;
};

uint get_array_index(uint id, uint num_nodes)
{
    uint level = get_msb(id + 1);
    uint level_id = id  - ((1 << level) - 1);
    return level_id + num_nodes - (1 << (level + 1)) + 1;
}

float squared_min_distance(vec3 p, vec3 bbox_min, vec3 bbox_max)
{
    vec3 d = min(max(bbox_min, p), bbox_max) - p;
    return dot(d,d);
}

float squared_max_distance(vec3 p, vec3 bbox_min, vec3 bbox_max)
{
    vec3 d = max(abs(bbox_min - p), abs(bbox_max - p));
    return dot(d,d);
}

// only using point lights 
// from Walter et. al. 2005
float geometric_term(vec3 p, vec3 bbox_min, vec3 bbox_max)
{
    float max_pz = max(bbox_min.z, bbox_max.z);
    if (max_pz >= 0) 
    {
        vec3 min_p2 = min(bbox_min*bbox_min, bbox_max*bbox_max);
        return max_pz * inversesqrt(min_p2.x + min_p2.y + max_pz*max_pz);
    }
    vec3 max_p2 = max(bbox_min*bbox_min, bbox_max*bbox_max);
    return max_pz * inversesqrt(max_p2.x + max_p2.y + max_pz*max_pz);
}

float calc_node_error(uint id, vec3 p)
{
    node_t node = nodes[id];
    float dmin2 = squared_min_distance(p, node.bbox_min, node.bbox_max);
    return geometric_term(p, node.bbox_min, node.bbox_max) * node.intensity / dmin2;
}

void gen_light_cut(vec3 p, inout light_cut_t light_cut[MAX_CUT_SIZE], int num_nodes, out uint selected)
{
    selected = 1;
    light_cut[0].id = 0;
    uint max_id = 0;
    uint h = get_msb(num_nodes + 1);
    // limit to number of leaf nodes
    uint size = min(USER_MAX_CUT, (1 << h));
    while (selected < size)
    {
        // replace with children
        uint id = uint(light_cut[max_id].id);
        uint lchild = ((id + 1) << 1) - 1;
        uint rchild = lchild + 1;

        light_cut[max_id].id = lchild;
        light_cut[max_id].error = calc_node_error(get_array_index(lchild, num_nodes), p);
    
        // second child added to the back
        uint ridx = get_array_index(rchild, num_nodes);
        if (nodes[ridx].intensity > 0)
        {
            light_cut[selected].id = rchild;
            light_cut[selected].error = calc_node_error(ridx, p);
            selected++;
        }

        // find node in current lightcut with highest error
        // to choose which child to replace with its children
        float max_error = FLT_MIN;
        for (int i = 0; i < selected; ++i)
        {
            light_cut_t n = light_cut[i];
            if (n.error > max_error)
            {
                max_id = i;
                max_error = n.error;
            }
        }
    }
}

// get list of light samples
void select_lights(vec3 p, 
    uint light_cut_size, 
    inout light_cut_t light_cut[MAX_CUT_SIZE], 
    inout selected_light_t selected_lights[MAX_CUT_SIZE], 
    int num_nodes,
    int num_leaf_nodes,
    float r)
{
    // for each node in the cut
    // - traverse tree and return light source and probability of sampling that light 
    //      (probability = (p_min + p_max)/2
    //      p_min = F(x,w)*Intensity/(d_min)^2 etc..
    // - calculate material term (brdf)
    // - calculate visibility
    // - color[i] = is_visible * light.color / probability
    
    // tree traversal
    uint leaf_node_max_id = num_nodes - num_leaf_nodes;
    for (uint i = 0; i < light_cut_size; i++)
    {
        uint id = light_cut[i].id;
        float prob = 1.0;
        // while not leaf node
        while (id < leaf_node_max_id) 
        {
            uint c0 = ((id + 1) << 1) - 1;
            uint c1 = c0 + 1;
            
            uint c0_idx = get_array_index(c0, num_nodes);
            uint c1_idx = get_array_index(c1, num_nodes);
            node_t n0 = nodes[c0_idx];
            node_t n1 = nodes[c1_idx];
           
            // todo check if bogus
            if (n0.intensity + n1.intensity == 0) 
            {
                id = INVALID_ID;
                break;
            }
            if (n0.intensity == 0)
            {
                id = c1;
                continue;
            }
            if (n1.intensity == 0)
            {
                id = c0;
                continue;
            }
            
            float g0 = geometric_term(p, n0.bbox_min, n0.bbox_max);
            float g1 = geometric_term(p, n1.bbox_min, n1.bbox_max);

            if (g0 + g1 == 0.0)
            {
                id = INVALID_ID;
                break;
            } 

            float gi0 = g0 * n0.intensity;
            float w0_min = gi0 / squared_min_distance(p, n0.bbox_min, n0.bbox_max);
            float w0_max = gi0 / squared_max_distance(p, n0.bbox_min, n0.bbox_max);
    
            float gi1 = g1 * n1.intensity;
            float w1_min = gi1 / squared_min_distance(p, n1.bbox_min, n1.bbox_max);
            float w1_max = gi1 / squared_max_distance(p, n1.bbox_min, n1.bbox_max);

            float prob_c0_max = w0_max / (w0_max + w1_max);
            float prob_c0_min = w0_min + w1_min == 0.0 ? gi0/(gi0 + gi1) : w0_min / (w0_min + w1_min);
            
            float prob_c0 = (prob_c0_max + prob_c0_min) * 0.5;
            if (r < prob_c0) 
            {
                prob *= prob_c0;
                r /= prob_c0;
                id = c0;
            }
            else 
            {
                prob *= (1 - prob_c0);
                r = (r - prob_c0)/(1 - prob_c0);
                id = c1;
            }
        }
        if (id != INVALID_ID)
        {
            id = get_array_index(id, num_nodes);
        }
        selected_lights[i].id = id;
        selected_lights[i].prob = prob;
    }
}
