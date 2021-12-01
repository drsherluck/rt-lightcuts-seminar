#ifndef MESH_H
#define MESH_H

#include "math.h"
#include "backend.h"

#include <string>
#include <vector>

struct vertex_t
{
	v3 position;
	v3 normal;
	v2 texcoord;
};

// array of structs
struct mesh_data_t
{
    std::vector<vertex_t> vertices;
    std::vector<u32>      indices;
};

// struct of arrays
struct mesh_data_2_t
{
    std::vector<u32> indices;
    std::vector<v3>  vertices;
    std::vector<v3>  normals;
    std::vector<v2>  texcoords;
};

void load_mesh_data(std::string path, mesh_data_t& mesh);
void load_mesh_data(std::string path, mesh_data_2_t& data);

void plane_mesh_data(mesh_data_t& mesh);

#endif 
