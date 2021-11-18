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

#define MAX_CUT_SIZE 10
// todo: make push constant
#define USER_MAX_CUT 3

// (hack) define the buffer here so that we can reuse
layout(std430, set = NODES_SSBO_SET, binding = 0) readonly buffer nodes_sbo
{
    node_t nodes[];
};

struct light_cut_t
{
    uint id;
    float error;
};

struct light_tree_info_t
{
    int num_nodes;
    int num_leaf_nodes;
};

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
float geometric_term(vec3 p, vec3 bbox_min, vec3 bbox_max)
{
    vec3 c = (bbox_min + bbox_max)/2.0;
    float d = length(c - p); 
    return 1.0/(d*d);
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
    int max_id = 0;
    while (selected < USER_MAX_CUT)
    {
        // replace with children
        uint lchild = ((max_id + 1) << 1) - 1;
        uint rchild = lchild + 1;

        light_cut[max_id].id = lchild;
        light_cut[max_id].error = calc_node_error(num_nodes - lchild, p);
    
        // second child added to the back
        if (nodes[num_nodes - rchild].intensity > 0)
        {
            selected++;
            light_cut[selected - 1].id = rchild;
            light_cut[selected - 1].error = calc_node_error(num_nodes - rchild, p);
        }

        // find node in current lightcut with highest error
        // to choose which child to replace with its children
        float max_error = FLT_MIN;
        for (int i = 0; i < selected; ++i)
        {
            if (light_cut[i].error > max_error)
            {
                max_id = i;
                max_error = light_cut[i].error;
            }
        }
        
        // stop if the error is acceptable 
        if (max_error < 0.02) break;
    }
}

struct selected_light_t
{
    uint id; // todo make int?
    float prob;
};

// get list of light samples
void select_lights(vec3 p, 
    uint light_cut_size, 
    inout light_cut_t light_cut[MAX_CUT_SIZE], 
    inout selected_light_t selected_lights[MAX_CUT_SIZE], 
    inout light_tree_info_t tree_info)
{
    // for each node in the cut
    // - traverse tree and return light source and probability of sampling that light 
    //      (probability = (p_min + p_max)/2
    //      p_min = F(x,w)*Intensity/(d_min)^2 etc..
    // - calculate material term (brdf)
    // - calculate visibility
    // - color[i] = is_visible * light.color / probability
    
    // tree traversal
    for (uint i = 0; i < light_cut_size; i++)
    {
        uint id = light_cut[i].id;
        uint leaf_node_max_id = tree_info.num_nodes - tree_info.num_leaf_nodes;

        // todo use time
        float r = random(float(id));
        float prob = 1.0;
        // while not leaf node
        while (id < leaf_node_max_id) 
        {
            uint c0 = ((id + 1) << 1) - 1;
            uint c1 = c0 + 1;
           
            node_t n0 = nodes[tree_info.num_nodes - c0];
            node_t n1 = nodes[tree_info.num_nodes - c1];
           
            // todo check if bogus
            if (n0.intensity + n1.intensity == 0) 
            {
                id = INVALID_ID;
                break;
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
            id = tree_info.num_nodes - id - 1;
        }
        selected_lights[i].id = id;
        selected_lights[i].prob = prob;
    }
}
