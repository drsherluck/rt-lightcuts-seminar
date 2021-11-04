#include "mesh.h"
#include "log.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <cassert>
#include <iostream>
#include <unordered_map>

u32 hash_index(u32 vertex_index, u32 normal_index, u32 texcoord_index)
{
	u32 hash = 17;
	hash = hash * 31 + vertex_index;
	hash = hash * 31 + normal_index;
	hash = hash * 31 + texcoord_index;
	return hash;
}

void load_mesh_data(std::string path, mesh_data_2_t& data)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	std::string warn;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

	if (!err.empty()) 
	{
		LOG_ERROR(err.data());
	}

	if (!warn.empty())
	{
		LOG_INFO(warn.data());
	}

	if (!ret)
	{
		exit(1);
	}

    assert(shapes.size() == 1);
	std::unordered_map<u32, u32> unique_map;

	for (size_t s = 0; s < shapes.size(); s++)
	{
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			//size_t fv = shapes[s].mesh.num_face_vertices[f];
			size_t fv = 3;

			for (size_t v = 0; v < fv; v++)
			{
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                assert(idx.vertex_index >= 0 && idx.normal_index >= 0 && idx.texcoord_index >= 0);

                v3 pos;
				pos.x = attrib.vertices[3*idx.vertex_index + 0];
				pos.y = attrib.vertices[3*idx.vertex_index + 1];
				pos.z = attrib.vertices[3*idx.vertex_index + 2];

                v3 normal;
                normal.x = attrib.normals[3*idx.normal_index + 0];
                normal.y = attrib.normals[3*idx.normal_index + 1];
                normal.z = attrib.normals[3*idx.normal_index + 2];

                v2 texcoord;
                texcoord.x = attrib.texcoords[2*idx.texcoord_index + 0];
                texcoord.y = attrib.texcoords[2*idx.texcoord_index + 1];

				// keeping vertices unique and indexing them
				u32 hash = hash_index(idx.vertex_index, idx.normal_index, idx.texcoord_index);
				if (unique_map.count(hash) == 0)
				{
					unique_map[hash] = data.vertices.size();
                    // add to data
					data.vertices.push_back(pos);
                    data.normals.push_back(normal);
                    data.texcoords.push_back(texcoord);
				}
				data.indices.push_back(unique_map[hash]);
			}

			index_offset += fv;
		}
	}
}

void load_mesh_data(std::string path, mesh_data_t& mesh)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	std::string warn;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

	if (!err.empty()) 
	{
		LOG_ERROR(err);
	}

	if (!warn.empty())
	{
		LOG_INFO(warn);
	}

	if (!ret)
	{
		exit(1);
	}

	std::unordered_map<u32, u32> unique_map;
	for (size_t s = 0; s < shapes.size(); s++)
	{
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			//size_t fv = shapes[s].mesh.num_face_vertices[f];
			size_t fv = 3;

			for (size_t v = 0; v < fv; v++)
			{
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                assert(idx.vertex_index >= 0 && idx.normal_index >= 0 && idx.texcoord_index >= 0);

                v3 pos;
				pos.x = attrib.vertices[3*idx.vertex_index + 0];
				pos.y = attrib.vertices[3*idx.vertex_index + 1];
				pos.z = attrib.vertices[3*idx.vertex_index + 2];

                v3 normal;
                normal.x = attrib.normals[3*idx.normal_index + 0];
                normal.y = attrib.normals[3*idx.normal_index + 1];
                normal.z = attrib.normals[3*idx.normal_index + 2];

                v2 texcoord;
                texcoord.x = attrib.texcoords[2*idx.texcoord_index + 0];
                texcoord.y = attrib.texcoords[2*idx.texcoord_index + 1];

				// keeping vertices unique and indexing them
				u32 hash = hash_index(idx.vertex_index, idx.normal_index, idx.texcoord_index);
				if (unique_map.count(hash) == 0)
				{
					unique_map[hash] = mesh.vertices.size();
					mesh.vertices.emplace_back(vertex_t{pos, normal, texcoord});
				}
				mesh.indices.push_back(unique_map[hash]);
			}
			index_offset += fv;
		}
	}
}

void plane_mesh_data(mesh_data_t& mesh)
{
    mesh.vertices.resize(4);
    mesh.vertices[0] = {vec3(-1,0,1), vec3(0,1,0), vec2(0,0)};
    mesh.vertices[1] = {vec3(1,0,1), vec3(0,1,0), vec2(0,1)};
    mesh.vertices[2] = {vec3(-1,0,-1), vec3(0,1,0), vec2(1,0)};
    mesh.vertices[3] = {vec3(1,0,-1), vec3(0,1,0), vec2(1,1)};
   
    u32 indices[6] = {0,1,2,2,1,3};
    mesh.indices.resize(6);
    mesh.indices.assign(indices, indices + 6);
}

