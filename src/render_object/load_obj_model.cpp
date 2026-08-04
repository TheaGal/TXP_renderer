#include "load_obj_model.h"

#include "btglm.h"
#include "btlogger.h"
#include "material_organizer/material_organizer.h"
#include "render_model.h"
#include "tiny_obj_loader.h"
#include "vertex.h"

#include <filesystem>
#include <unordered_map>


void TXP::load_obj_model_from_disk(Render_model_data_collection& data_collection,
                                   Material_organizer& material_organizer,
                                   std::string const& model_name,
                                   std::string const& fname)
{
    if (!std::filesystem::exists(fname) ||
        !std::filesystem::is_regular_file(fname))
    {
        // Exit early if this isn't a good fname.
        throw std::runtime_error("FFFFFFFFFF");
        // logger::printef(logger::ERROR, "\"%s\" does not exist or is not a file.", fname.c_str());
        // assert(false);
        // return;
    }

    // Load obj file.
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string base_dir{ std::filesystem::path(fname).parent_path().generic_string() };

    std::string warn;
    std::string err;
    if (!tinyobj::LoadObj(&attrib,
                          &shapes,
                          &materials,
                          &warn,
                          &err,
                          fname.c_str(),
                          base_dir.c_str()))
    {
        throw std::runtime_error("FFFFFFFFFF");
        // logger::printef(logger::ERROR, "OBJ file \"%s\" failed to load.", fname.c_str());
        // assert(false);
        // return;
    }

    if (!warn.empty())
    {
        throw std::runtime_error("FFFFFFFFFF");
        // logger::printe(logger::WARN, warn);
        // assert(false);
    }

    if (!err.empty())
    {
        throw std::runtime_error("FFFFFFFFFF");
        // logger::printe(logger::ERROR, err);
        // assert(false);
    }

    // Create default material palette for this model.
    bool has_non_default_material_in_set{ false };
    std::vector<std::string> material_palette_material_names;
    for (auto& shape : shapes)
    {
        if (shape.mesh.indices.empty())
        {
            // Skip non-mesh shapes.
            continue;
        }

        int32_t material_idx{ shape.mesh.material_ids.front() };
        if (material_idx < 0)
            material_palette_material_names.emplace_back("default_mat");
        else
        {
            material_palette_material_names.emplace_back(materials[material_idx].name);
            has_non_default_material_in_set = true;
        }
    }
    if (has_non_default_material_in_set)
    {   // Create new material palette.
        Material_palette new_mat_pal;
        new_mat_pal.emplace_materials(material_organizer, material_palette_material_names);
        material_organizer.emplace_material_palette(model_name +
                                                        "__default_material_palette_name__",
                                                    std::move(new_mat_pal));
    }
    else  // Use default material palette.
        material_organizer.emplace_material_palette_alias(model_name +
                                                              "__default_material_palette_name__",
                                                          "default_material_palette");

    // Get all unique combinations of attributes together.
    assert(attrib.vertices.size() < std::pow(2, 21));
    assert(attrib.normals.size() < std::pow(2, 21));
    assert(attrib.texcoords.size() < std::pow(2, 21));
    auto generate_key_fn = [](tinyobj::index_t const& index) {
        assert(index.vertex_index >= 0);
        assert(index.normal_index >= 0);
        assert(index.texcoord_index >= 0);
        uint64_t key{ (static_cast<uint64_t>(index.vertex_index) << 42) |
                      (static_cast<uint64_t>(index.normal_index) << 21) |
                      (static_cast<uint64_t>(index.texcoord_index) << 0) };
        return key;
    };

    struct Vertex_with_index
    {
        uint32_t index;
        Vertex vertex;
    };

    std::unordered_map<uint64_t, Vertex_with_index> key_to_vertex_map;
    uint32_t current_index{ 0 };
    for (auto& shape : shapes)
    for (auto& index : shape.mesh.indices)
    {
        // Generate and emplace key if unique.
        uint64_t key = generate_key_fn(index);
        
        if (key_to_vertex_map.find(key) == key_to_vertex_map.end())
        {
            // Add new vertex.
            Vertex new_gpu_vertex;
            glm_vec3_copy(vec3{ attrib.vertices[3 * index.vertex_index + 0],
                                attrib.vertices[3 * index.vertex_index + 1],
                                attrib.vertices[3 * index.vertex_index + 2] }, new_gpu_vertex.position_vec3());
            glm_vec3_copy(vec3{ attrib.normals[3 * index.normal_index + 0],
                                attrib.normals[3 * index.normal_index + 1],
                                attrib.normals[3 * index.normal_index + 2] }, new_gpu_vertex.normal_vec3());
            glm_vec2_copy(vec2{ attrib.texcoords[2 * index.texcoord_index + 0],
                                attrib.texcoords[2 * index.texcoord_index + 1] }, new_gpu_vertex.uv_vec2());

            Vertex_with_index vwi{ current_index++, new_gpu_vertex };
            key_to_vertex_map.emplace(key, vwi);
        }
    }

    // New static model to insert data into.
    Static_model_data_set new_static_model_data_set;

    auto& meshes{ new_static_model_data_set.meshes };
    auto& vertices{ new_static_model_data_set.vertices };
    auto& model_aabb{ new_static_model_data_set.model_aabb };
    auto& first_index_offsets{ new_static_model_data_set.first_index_offsets };

    // Transform vertices into model structure.
    vertices.clear();
    vertices.resize(key_to_vertex_map.size());

    for (auto it = key_to_vertex_map.begin(); it != key_to_vertex_map.end(); it++)
    {
        vertices[it->second.index] = it->second.vertex;
    }

    // Find whole AABB.
    model_aabb.reset();
    for (auto& vertex : vertices)
    {
        model_aabb.feed_position(vertex.position_vec3());
    }

#define OPENGL_SPECIFIC_STUFF 0
#if OPENGL_SPECIFIC_STUFF
    // Upload vertices to GPU.
    glGenVertexArrays(1, &m_model_vertex_vao);
    glGenBuffers(1, &m_model_vertex_vbo);

    glBindVertexArray(m_model_vertex_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_model_vertex_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STATIC_DRAW);

    // Register vertex attributes.
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, tex_coord)));

    // Unbind.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
#endif // OPENGL_SPECIFIC_STUFF

    // Transform indices into mesh structures.
    meshes.clear();
    meshes.reserve(shapes.size());

    first_index_offsets.reserve(meshes.size());  // @NOTE: reserved now for memory continuity on heap.

    for (auto& shape : shapes)
    {
        std::vector<uint32_t> indices;
        indices.reserve(shape.mesh.indices.size());
        for (auto& index : shape.mesh.indices)
        {
            uint64_t key = generate_key_fn(index);
            indices.emplace_back(key_to_vertex_map.at(key).index);
        }

        // Create mesh.
        meshes.emplace_back(shape.name, std::move(indices));
    }

    // Place data into collection.
    data_collection.emplace_static_model_data_set(model_name, std::move(new_static_model_data_set));
}
