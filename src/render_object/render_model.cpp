#include "render_model.h"

#include "cglm/cglm.h"
#include "cglm/struct.h"
#include "cglm/vec3.h"
#include "fastgltf/core.hpp"
#include "fastgltf/math.hpp"
#include "fastgltf/types.hpp"
#include "fastgltf/tools.hpp"
#include "render_object/deformed_render_model.h"
#include "tiny_obj_loader.h"
#include "vertex.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <list>
#include <stdexcept>
#include <vector>


namespace
{

static std::string s_model_directory;

}  // namespace


void TXP::set_model_directory(std::string const& dir_path)
{
    s_model_directory = dir_path;
}


// AA_bounding_box
void TXP::AA_bounding_box::reset()
{
    min[0] = min[1] = min[2] = std::numeric_limits<float_t>::max();
    max[0] = max[1] = max[2] = std::numeric_limits<float_t>::lowest();
}

void TXP::AA_bounding_box::feed_position(vec3 position)
{
    glm_vec3_minv(min, position, min);
    glm_vec3_maxv(max, position, max);
}


namespace TXP
{

Render_model load_obj_model_from_disk(std::string const& fname)
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
    std::vector<tinyobj::material_t> materials;  // @NOTE: Materials are simply ignored (for obj files).

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
        meshes.emplace_back(std::move(indices));
    }

    
}

Render_model load_gltf_model_from_disk(std::string const& fname)
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

    fastgltf::Asset asset;
    {
        // Parse gltf file into asset data structure.
        fastgltf::Parser parser{ fastgltf::Extensions::None };

        constexpr auto k_gltf_options{
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::DecomposeNodeMatrices |  // To ensure node trans is TRS variant.
            fastgltf::Options::GenerateMeshIndices };
        
        auto gltf_file{ fastgltf::MappedGltfFile::FromPath(fname) };
        if (!bool(gltf_file))
        {
            throw std::runtime_error("FFFFFFFFFF");
            // logger::printef(logger::ERROR,
            //                 "Failed to open glTF file: %s (err msg: %s)",
            //                 fname.c_str(),
            //                 fastgltf::getErrorMessage(gltf_file.error()));
            // assert(false);
            // return;
        }

        auto possible_asset{
            parser.loadGltf(gltf_file.get(),
                            std::filesystem::path{ fname }.parent_path(),
                            k_gltf_options) };
        if (possible_asset.error() != fastgltf::Error::None)
        {
            throw std::runtime_error("FFFFFFFFFF");
            // logger::printef(logger::ERROR,
            //                 "Failed to load glTF asset from file: %s (err msg: %s)",
            //                 fname.c_str(),
            //                 fastgltf::getErrorMessage(possible_asset.error()));
            // assert(false);
            // return;
        }

        asset = std::move(possible_asset.get());
    }

    // Calculate all nodes' global transform.
    std::unordered_map<size_t, mat4s> node_idx_to_global_transform_map;
    {
        std::vector<size_t> root_node_indices;
        {   // Build list of root nodes.
            std::vector<bool> per_node_is_root_node;
            per_node_is_root_node.resize(asset.nodes.size(), true);
            for (auto& node : asset.nodes)
            {   // Mark found children nodes as not a root node.
                for (size_t child_idx : node.children)
                    per_node_is_root_node[child_idx] = false;
            }

            // Return list of root nodes.
            for (size_t i = 0; i < per_node_is_root_node.size(); i++)
                if (per_node_is_root_node[i])
                    root_node_indices.emplace_back(i);
        }

        // Calc all global transforms.
        // @NOTE: Queue method is used to accomplish breadth first.
        struct Process_node
        {
            size_t parent_idx;
            size_t my_idx;
        };
        constexpr size_t k_invalid{ (size_t)-1 };

        std::list<Process_node> process_nodes;
        for (size_t root_node_idx : root_node_indices)
            process_nodes.emplace_back(k_invalid, root_node_idx);

        while (!process_nodes.empty())
        {   // Process front node in queue.
            auto process_node{ process_nodes.front() };
            process_nodes.pop_front();

            mat4 parent_transform = GLM_MAT4_IDENTITY_INIT;
            if (process_node.parent_idx != k_invalid)
            {   // Read parent transform.
                glm_mat4_copy(
                    node_idx_to_global_transform_map.at(process_node.parent_idx).raw,
                    parent_transform);
            }

            auto trs{ std::get<fastgltf::TRS>(asset.nodes[process_node.my_idx].transform) };
            mat4s node_transform;
            glm_translate_make(node_transform.raw, trs.translation.data());
            glm_quat_rotate(node_transform.raw,
                            versor{ trs.rotation.x(),  // Quat copy uses SIMD which conflicts
                                    trs.rotation.y(),  // memory-wise with the fastgltf setup.
                                    trs.rotation.z(),
                                    trs.rotation.w() },
                            node_transform.raw);
            glm_scale(node_transform.raw, trs.scale.data());

            glm_mat4_mul(parent_transform, node_transform.raw, node_transform.raw);
            // glm_mat4_mul(node_transform.raw, parent_transform, node_transform.raw);

            node_idx_to_global_transform_map.emplace(process_node.my_idx,
                                                     std::move(node_transform));

            // Add children as process nodes.
            for (size_t child_node_idx : asset.nodes[process_node.my_idx].children)
                process_nodes.emplace_back(process_node.my_idx, child_node_idx);
        }
    }

    // Data structures to load into.
    Deformed_model_skin new_deformed_model_skin;

    auto& vert_skin_datas{ new_deformed_model_skin.vert_skin_datas };

    Static_model_data_set new_static_model_data_set;

    auto& meshes{ new_static_model_data_set.meshes };
    auto& vertices{ new_static_model_data_set.vertices };
    auto& model_aabb{ new_static_model_data_set.model_aabb };

    // Load skins.
    std::unordered_map<size_t, size_t> node_idx_to_model_joint_idx_map;  // @NOTE: Need for rest of loading procs.
    std::unordered_map<size_t, size_t> gltf_asset_joint_idx_to_insert_order_map;  // @NOTE: For remapping joint indices.
    std::vector<size_t> node_index_insert_order;  // @NOTE: Need for animation creation.

    if (asset.skins.size() > 1)
    {
        throw std::runtime_error("FFFFFFFFFF");
        // logger::printef(logger::WARN,
        //                 "glTF asset has more than 1 skin when only 1 skin is supported. Skins: %lld",
        //                 asset.skins.size());
        // assert(false);  // For debug purposes.
    }

    bool first_skin_w_joints{ true };
    for (auto& skin : asset.skins)
    if (!skin.joints.empty())
    {   // Ensure that only one skin is processed.
        assert(first_skin_w_joints);
        first_skin_w_joints = false;

        // Load all joint data.
        std::vector<mat4s> inv_bind_mats;
        {   // Load all inverse bind matrices.
            auto& inv_bind_mats_accessor{
                asset.accessors[skin.inverseBindMatrices.value()] };
            for (auto element :
                     fastgltf::iterateAccessor<fastgltf::math::fmat4x4>(asset,
                                                                        inv_bind_mats_accessor))
            {
                // @NOTE: `_ucopy()` is used instead of the normal copy to force no SIMD.
                mat4s new_inv_bind_mat;
                glm_mat4_ucopy(reinterpret_cast<vec4*>(element.data()), new_inv_bind_mat.raw);
                inv_bind_mats.emplace_back(std::move(new_inv_bind_mat));
            }
        }

        size_t root_joint_node_idx;
        std::unordered_map<size_t, size_t> node_idx_to_inv_bind_mat_idx_map;
        {   // Build child-to-parent map and node-to-inverse_bind_matrices map.
            std::unordered_map<size_t, size_t> child_to_parent_map;
            size_t inv_bind_mat_idx{ 0 };
            for (auto joint_node_idx : skin.joints)
            {
                node_idx_to_inv_bind_mat_idx_map.emplace(joint_node_idx, inv_bind_mat_idx);
                for (auto child_node_idx : asset.nodes[joint_node_idx].children)
                {
                    if (std::find(skin.joints.begin(), skin.joints.end(), child_node_idx)
                        == skin.joints.end())
                    {   // Child of the joint node is not a joint node.
                        throw std::runtime_error("FFFFFFFFFF");
                        // logger::printe(logger::ERROR, "Child of joint node is not a joint node.");
                        // assert(false);
                        // return;
                    }
                    child_to_parent_map.emplace(child_node_idx, joint_node_idx);
                }
                inv_bind_mat_idx++;
            }

            // Find joint root node.
            root_joint_node_idx = skin.joints.front();
            while (child_to_parent_map.find(root_joint_node_idx) != child_to_parent_map.end())
                root_joint_node_idx = child_to_parent_map.at(root_joint_node_idx);
        }

        {   // Calc root node inverse global transform.
            // Find skin node idx.
            size_t skin_idx{ (size_t)-1 };
            for (size_t i = 0; i < asset.skins.size(); i++)
                if (&asset.skins[i] == &skin)
                {
                    skin_idx = i;
                    break;
                }

            size_t skin_node_idx{ (size_t)-1 };
            for (size_t i = 0; i < asset.nodes.size(); i++)
                if (asset.nodes[i].skinIndex.has_value() &&
                    asset.nodes[i].skinIndex.value() == skin_idx)
                {
                    skin_node_idx = i;
                    break;
                }

            // Spit out skin node inverse transform.
            glm_mat4_inv_precise(node_idx_to_global_transform_map.at(skin_node_idx).raw,
                                 new_deformed_model_skin.inverse_global_transform);
        }

        {   // Calc full child to parent node map.
            std::unordered_map<size_t, size_t> child_to_parent_node_idx_map;
            for (size_t parent_node_idx = 0; parent_node_idx < asset.nodes.size(); parent_node_idx++)
                for (size_t child_node_idx : asset.nodes[parent_node_idx].children)
                {
                    child_to_parent_node_idx_map.emplace(child_node_idx, parent_node_idx);
                }

            // Calc baseline transform.
            if (child_to_parent_node_idx_map.find(root_joint_node_idx) != child_to_parent_node_idx_map.end())
            {
                glm_mat4_copy(
                    node_idx_to_global_transform_map.at(
                        child_to_parent_node_idx_map.at(root_joint_node_idx)).raw,
                    new_deformed_model_skin.baseline_transform);
            }
            else
            {
                glm_mat4_identity(new_deformed_model_skin.baseline_transform);
            }
        }

        std::unordered_map<size_t, size_t> node_idx_to_gltf_joint_idx_map;
        for (size_t gji = 0; gji < skin.joints.size(); gji++)
        {   // Create mapping from node idx to gltf joint idx.
            node_idx_to_gltf_joint_idx_map.emplace(skin.joints[gji], gji);
        }

        {   // Write model joints into model skin.
            struct Node_process_job
            {
                size_t node_idx;
                size_t inv_bind_mat_idx;
            };
            std::list<Node_process_job> process_jobs;

            // Enter root job.
            process_jobs.emplace_back(root_joint_node_idx,
                                      node_idx_to_inv_bind_mat_idx_map.at(root_joint_node_idx));

            // Process jobs while adding more in a breadth-first way.
            node_idx_to_model_joint_idx_map.clear();
            gltf_asset_joint_idx_to_insert_order_map.clear();
            node_index_insert_order.clear();
            while (!process_jobs.empty())
            {
                auto job{ process_jobs.front() };
                process_jobs.pop_front();

                size_t next_joints_sorted_breadth_first_idx{
                    new_deformed_model_skin.joints_sorted_breadth_first.size()
                };

                node_idx_to_model_joint_idx_map.emplace(
                    job.node_idx,
                    next_joints_sorted_breadth_first_idx);

                Model_joint new_model_joint{
                    std::string(asset.nodes[job.node_idx].name), };
                glm_mat4_copy(inv_bind_mats[job.inv_bind_mat_idx].raw,
                              new_model_joint.inverse_bind_matrix);
                // @NOTE: Add parent-child relation later.

                new_deformed_model_skin.joint_name_to_idx.emplace(new_model_joint.name,
                                                       next_joints_sorted_breadth_first_idx);
                new_deformed_model_skin.joints_sorted_breadth_first.emplace_back(new_model_joint);

                gltf_asset_joint_idx_to_insert_order_map.emplace(
                    node_idx_to_gltf_joint_idx_map.at(job.node_idx),
                    node_index_insert_order.size());
                node_index_insert_order.emplace_back(job.node_idx);

                for (auto child_node_idx : asset.nodes[job.node_idx].children)
                {
                    process_jobs.emplace_back(child_node_idx,
                                              node_idx_to_inv_bind_mat_idx_map.at(
                                                  child_node_idx));
                }
            }
            // Emplace for zero case (if zero case already exists then nothing happens with `emplace()`).
            gltf_asset_joint_idx_to_insert_order_map.emplace(0, 0);

            // Add parent-child relationships.
            assert(new_deformed_model_skin.joints_sorted_breadth_first.size() == node_index_insert_order.size());
            for (size_t node_idx : node_index_insert_order)
                for (size_t child_idx : asset.nodes[node_idx].children)
                {
                    // Establish parent-child relation.
                    size_t parent_model_joint_idx{ node_idx_to_model_joint_idx_map.at(node_idx) };
                    size_t child_model_joint_idx{ node_idx_to_model_joint_idx_map.at(child_idx) };

                    auto& parent_joint{ new_deformed_model_skin.joints_sorted_breadth_first[parent_model_joint_idx] };
                    auto& child_joint{ new_deformed_model_skin.joints_sorted_breadth_first[child_model_joint_idx] };

                    child_joint.parent_idx = parent_model_joint_idx;
                    parent_joint.children.emplace_back(&child_joint);
                }
        }

        // @NOTE: Ignore the `skeleton` property in the `Skin` struct.
    }

    // Load meshes.
    bool overall_has_skin{ !asset.skins.empty() };
    vertices.clear();
    model_aabb.reset();

    size_t num_meshes{ 0 };
    for (auto& mesh : asset.meshes)
        num_meshes += mesh.primitives.size();

    meshes.clear();
    meshes.reserve(num_meshes);  // @NOTE: Reserve prevents calling dtor() which messes up the meshes.

    for (auto& mesh : asset.meshes)
        for (auto& primitive : mesh.primitives)
        {   // Load vertices.
            // Find all wanted accessors.
            auto pos_attribute{ primitive.findAttribute("POSITION") };
            auto norm_attribute{ primitive.findAttribute("NORMAL") };
            auto tex_coord_attribute{ primitive.findAttribute("TEXCOORD_0") };
            auto joints_attribute{ primitive.findAttribute("JOINTS_0") };
            auto weights_attribute{ primitive.findAttribute("WEIGHTS_0") };

            assert(pos_attribute != nullptr);  // POSITION is definitely required.
            assert(norm_attribute != nullptr);
            assert(tex_coord_attribute != nullptr);
            assert((joints_attribute != nullptr) == (weights_attribute != nullptr));

            auto& pos_accessor{ asset.accessors[pos_attribute->accessorIndex] };
            auto& norm_accessor{ asset.accessors[norm_attribute->accessorIndex] };
            auto& tex_coord_accessor{ asset.accessors[tex_coord_attribute->accessorIndex] };
            fastgltf::Accessor* joints_accessor{ nullptr };
            fastgltf::Accessor* weights_accessor{ nullptr };
            bool has_skin{ false };

            if (joints_attribute != nullptr && weights_attribute != nullptr)
            {   // Include skinning accessors.
                joints_accessor = &asset.accessors[joints_attribute->accessorIndex];
                weights_accessor = &asset.accessors[weights_attribute->accessorIndex];
                has_skin = true;
            }

            // Either all meshes must have/not have a skin, with the exception of
            // overall meshes having skins but this one in particular doesn't.
            // A dummy set of skin weights will be applied later for this exception.
            assert(overall_has_skin == has_skin ||
                   (overall_has_skin && !has_skin));

            // Resize to include new vertices.
            auto base_vertex_idx{ vertices.size() };
            vertices.resize(base_vertex_idx + pos_accessor.count);
            if (overall_has_skin)
            {   // Include vertex skin data even if this mesh does not have skin.
                vert_skin_datas.resize(base_vertex_idx + pos_accessor.count);
            }

            // Load data for new vertices.
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, pos_accessor,
                [&model_aabb, &vertices, base_vertex_idx](fastgltf::math::fvec3 v, size_t index) {
                    model_aabb.feed_position(v.data());
                    glm_vec3_copy(v.data(),
                                  vertices[base_vertex_idx + index].position_vec3());
                });

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, norm_accessor,
                [&vertices, base_vertex_idx](fastgltf::math::fvec3 v, size_t index) {
                    glm_vec3_copy(v.data(),
                                  vertices[base_vertex_idx + index].normal_vec3());
                });

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset, tex_coord_accessor,
                [&vertices, base_vertex_idx](fastgltf::math::fvec2 v, size_t index) {
                    glm_vec2_copy(v.data(),
                                  vertices[base_vertex_idx + index].uv_vec2());
                });

            if (has_skin)
            {   // Joint indices.
                switch (joints_accessor->componentType)
                {
                    case fastgltf::ComponentType::UnsignedByte:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec4>(asset, *joints_accessor,
                            [&vert_skin_datas, base_vertex_idx, &gltf_asset_joint_idx_to_insert_order_map]
                            (fastgltf::math::u8vec4 v, size_t index) {
                                vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[0] =
                                    gltf_asset_joint_idx_to_insert_order_map.at(v.x());
                                vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[1] =
                                    gltf_asset_joint_idx_to_insert_order_map.at(v.y());
                                vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[2] =
                                    gltf_asset_joint_idx_to_insert_order_map.at(v.z());
                                vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[3] =
                                    gltf_asset_joint_idx_to_insert_order_map.at(v.w());
                            });
                        break;

                    case fastgltf::ComponentType::UnsignedShort:
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec4>(asset, *joints_accessor,
                            [&vert_skin_datas, base_vertex_idx, &gltf_asset_joint_idx_to_insert_order_map]
                            (fastgltf::math::u16vec4 v, size_t index) {
                                vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[0] =
                                    gltf_asset_joint_idx_to_insert_order_map.at(v.x());
                                vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[1] =
                                    gltf_asset_joint_idx_to_insert_order_map.at(v.y());
                                vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[2] =
                                    gltf_asset_joint_idx_to_insert_order_map.at(v.z());
                                vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[3] =
                                    gltf_asset_joint_idx_to_insert_order_map.at(v.w());
                            });
                        break;

                    default:
                        // Component type for joint indices not supported.
                        throw std::runtime_error("FJFJJFJFJF");
                        // assert(false);
                        // return;
                }

                // Weights.
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(asset, *weights_accessor,
                    [&vert_skin_datas, base_vertex_idx](fastgltf::math::fvec4 v, size_t index) {
                        glm_vec4_copy(v.data(),
                                      vert_skin_datas[base_vertex_idx + index].weights);
                    });
            }
            else if (overall_has_skin && !has_skin)
            {   // Joint indices (dummy).
                for (size_t index = 0; index < pos_accessor.count; index++)
                {
                    vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[0] = 0;
                    vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[1] = 0;
                    vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[2] = 0;
                    vert_skin_datas[base_vertex_idx + index].joint_mat_idxs[3] = 0;
                }

                // Weights (dummy).
                for (size_t index = 0; index < pos_accessor.count; index++)
                {
                    glm_vec4_zero(vert_skin_datas[base_vertex_idx + index].weights);
                }
            }

            // Load all indices.
            assert(primitive.indicesAccessor.has_value());
            auto& indices_accessor{ asset.accessors[primitive.indicesAccessor.value()] };

            std::vector<uint32_t> indices;
            indices.reserve(indices_accessor.count);

            for (uint32_t ind : fastgltf::iterateAccessor<uint32_t>(asset, indices_accessor))
            {
                // Offset indices to ensure they're referencing the correct
                // mesh.
                indices.emplace_back(base_vertex_idx + ind);
            }

            // Create mesh in model.
            meshes.emplace_back(std::move(indices));
        }

#define OPENGL_SPECIFIC_STUFF 0
#if OPENGL_SPECIFIC_STUFF
    {   // Upload vertices to GPU.
        glGenVertexArrays(1, &m_model_vertex_vao);
        glGenBuffers(1, &m_model_vertex_vbo);

        glBindVertexArray(m_model_vertex_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_model_vertex_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

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

        if (overall_has_skin)
        {   // Upload vertex skin datas to GPU as well.
            glGenBuffers(1, &m_model_vertex_skin_datas_buffer);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_model_vertex_skin_datas_buffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         vert_skin_datas.size() * sizeof(Vertex_skin_data),
                         vert_skin_datas.data(),
                         GL_STATIC_READ);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
    }
#endif // OPENGL_SPECIFIC_STUFF

    // Load animations.
    m_animations.clear();
    m_animations.reserve(asset.animations.size());
    for (auto& anim : asset.animations)
    {
        // Set anim name.
        std::string anim_name{ anim.name };
        if (anim_name.empty())
        {
            anim_name = std::to_string(m_animations.size());
        }

        // Extract glTF-style animation data.
        struct Sampler_extracted_data
        {
            fastgltf::AnimationInterpolation interp_type;  // @NOTE: 永久に使われないかも。
            std::vector<float_t> times;
            std::vector<vec4s> trs_fragments;  // Could be trans, rot, or sca.
        };
        std::unordered_map<size_t, Sampler_extracted_data> sampler_idx_to_data_map;

        struct Channel_extracted_data
        {
            Sampler_extracted_data* sampler{ nullptr };
            size_t target_joint_idx;  // Idx in `joints_sorted_breadth_first`.
            fastgltf::AnimationPath trs_type;
        };
        std::vector<Channel_extracted_data> channel_datas;

        bool animation_data_invalid{ false };

        {   // Organize samplers.
            for (size_t i = 0; i < anim.samplers.size(); i++)
            {
                auto& sampler{ anim.samplers[i] };

                Sampler_extracted_data new_data;

                new_data.interp_type = sampler.interpolation;
                if (new_data.interp_type == fastgltf::AnimationInterpolation::Step)
                {
                    logger::printe(logger::WARN,
                                   "`Step` animation interpolation type not supported. May be "
                                   "supported in the future but for now it will just be imported as `Linear`.");
                    assert(false);
                }
                if (new_data.interp_type == fastgltf::AnimationInterpolation::CubicSpline)
                {
                    logger::printe(logger::ERROR,
                                   "`CubicSpline` animation interpolation type not supported.");
                    assert(false);
                    return;
                }

                {   // Get `.times` (sampler input).
                    auto& input_accessor{ asset.accessors[sampler.inputAccessor] };
                    assert(input_accessor.componentType == fastgltf::ComponentType::Float);
                    for (float_t element :
                         fastgltf::iterateAccessor<float_t>(asset, input_accessor))
                    {
                        new_data.times.emplace_back(element);
                    }
                }

                {   // Get `.trs_fragments` (sampler output).
                    auto& output_accessor{ asset.accessors[sampler.outputAccessor] };
                    assert(output_accessor.componentType == fastgltf::ComponentType::Float);

                    switch (output_accessor.type)
                    {
                        case fastgltf::AccessorType::Vec3:
                            for (fastgltf::math::fvec3 element :
                                 fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset,
                                                                                  output_accessor))
                            {
                                new_data.trs_fragments.emplace_back(vec4s{ element.x(),
                                                                           element.y(),
                                                                           element.z(),
                                                                           0.0f });
                            }
                            break;

                        case fastgltf::AccessorType::Vec4:
                            for (fastgltf::math::fvec4 element :
                                 fastgltf::iterateAccessor<fastgltf::math::fvec4>(asset,
                                                                                  output_accessor))
                            {
                                new_data.trs_fragments.emplace_back(vec4s{ element.x(),
                                                                           element.y(),
                                                                           element.z(),
                                                                           element.w() });
                            }
                            break;

                        default:
                            // Huh???
                            assert(false);
                            return;
                    }
                    // @NOTE: Please don't call me lazy but I just didn't want a big branch between
                    //   two `for` loops that would look pretty much the same  >.<
                    //     -Thea 2025/07/12 (I turned in my passport app w/ the correct gender today!
                    //                       Hopefully I don't get struck down by the tyrants)
                    // @AMEND: Turns out that I had to split it between two branches bc `iterateAccessor`
                    //   didn't like the `float_t` type for the accessor.  -Thea 2025/07/14
                }

                // Emplace.
                sampler_idx_to_data_map.emplace(i, new_data);
            }

            // Check for sampler times sorted.
            for (auto& sampler : sampler_idx_to_data_map)
            {
                float_t prev_time{ std::numeric_limits<float_t>::lowest() };
                for (float_t time : sampler.second.times)
                {
                    if (prev_time >= time)
                    {
                        logger::printef(logger::ERROR,
                                        "Times are not sorted asc! prev: %.6f curr: %.6f",
                                        prev_time,
                                        time);
                        assert(false);
                        return;  // Abort loading.
                    }
                    prev_time = time;
                }
            }

            // Organize channels.
            for (auto& channel : anim.channels)
            {
                if (node_idx_to_model_joint_idx_map.find(channel.nodeIndex.value())
                    == node_idx_to_model_joint_idx_map.end())
                {   // This channel is found to reference an invalid node. Handle by skipping this animation.
                    animation_data_invalid = true;
                }
                else
                {   // Add channel information to data extraction struct.
                    channel_datas.emplace_back(&sampler_idx_to_data_map.at(channel.samplerIndex),
                                               node_idx_to_model_joint_idx_map.at(
                                                   channel.nodeIndex.value()),
                                               channel.path);
                }
            }
        }

        if (animation_data_invalid)
        {   // Abort trying to import this animation and skip to next one.
            logger::printef(logger::WARN,
                            "Animation data for anim \"%s\" is invalid. Skipping.",
                            anim_name.c_str());
            continue;
        }

        // Convert glTF-style data to `Model_animator` data.
        constexpr float_t k_anim_frame_time{
            1.0f / Model_joint_animation::k_frames_per_second };
        float_t start_time{ std::numeric_limits<float_t>::max() };
        float_t end_time{ std::numeric_limits<float_t>::lowest() };
        uint32_t perceived_frames{ 0 };
        {   // Find start/end times of all samplers.
            for (auto& elem : sampler_idx_to_data_map)
            {
                auto& sampler{ elem.second };
                for (float_t time : sampler.times)
                {
                    start_time = std::min(start_time, time);
                    end_time = std::max(end_time, time);
                }
            }

            // Calculate the frames between the times.
            float_t num_frames_raw{ ((end_time - start_time) / k_anim_frame_time) + 1.0f };
            float_t deviation{ num_frames_raw - std::roundf(num_frames_raw) };
            if (abs(deviation) > 1e-6f)
            {
                logger::printef(logger::WARN,
                                "(model: \"%s\", anim_name: \"%s\") Animation length does not "
                                "match the %.3f hz animation cutting requirement "
                                "(deviation: %0.6f). Will extend animation clip until the cutting "
                                "requirement is fulfilled.",
                                fname.c_str(),
                                anim_name.c_str(),
                                Model_joint_animation::k_frames_per_second,
                                deviation);
                // @NOCHECKIN: Don't assert on this warning for now.
                // assert(false);  // Idk if you want an assert on this, but it's a heavier warning.
            }

            // Turn into perceived frames.
            perceived_frames = std::ceilf(num_frames_raw);
        }

        std::vector<Model_joint_animation_frame> new_anim_frames;
        new_anim_frames.reserve(perceived_frames);
        {   // Record animation frames.
            float_t curr_time{ start_time };

            for (size_t _ = 0; _ < perceived_frames; _++)
            {   // Get interpolation of current frame.
                std::unordered_map<size_t, Model_joint_animation_frame::Joint_local_transform>
                    joint_idx_to_local_trans_map;
                vec3 root_pos;

                for (auto const& channel : channel_datas)
                {
                    if (joint_idx_to_local_trans_map.find(channel.target_joint_idx)
                            == joint_idx_to_local_trans_map.end())
                    {   // @NOTE: Emplace in the beginning since channels only
                        // sample one part of the TRS.
                        joint_idx_to_local_trans_map.emplace(
                            channel.target_joint_idx,
                            Model_joint_animation_frame::Joint_local_transform{});
                    }

                    Model_joint_animation_frame::Joint_local_transform& joint_trans{
                        joint_idx_to_local_trans_map.at(channel.target_joint_idx) };

                    bool found_sample{ false };
                    assert(channel.sampler->times.size() >= 2);
                    for (size_t i = 0; i < channel.sampler->times.size() - 1; i++)
                        if (curr_time >= channel.sampler->times[i] &&
                            curr_time <= channel.sampler->times[i + 1])
                        {
                            float_t interp_t{ std::max(0.0f, curr_time - channel.sampler->times[i])
                                                  / (channel.sampler->times[i + 1] - channel.sampler->times[i]) };
                            auto const& output0{ channel.sampler->trs_fragments[i] };
                            auto const& output1{ channel.sampler->trs_fragments[i + 1] };
                            switch (channel.trs_type)
                            {
                                case fastgltf::AnimationPath::Translation:
                                    glm_vec3_lerp(const_cast<float_t*>(output0.raw),
                                                  const_cast<float_t*>(output1.raw),
                                                  interp_t,
                                                  joint_trans.position);

                                    if (channel.target_joint_idx == 0)
                                    {   // Root bone detected. Insert into root position.
                                        glm_vec3_copy(joint_trans.position, root_pos);
                                    }
                                    break;

                                case fastgltf::AnimationPath::Rotation:
                                    // @NOTE: Using `slerp()` for better accuracy than `nlerp()`.
                                    //   This is fine since it's just the import stage, not actual
                                    //   animation.
                                    glm_quat_slerp(const_cast<float_t*>(output0.raw),
                                                   const_cast<float_t*>(output1.raw),
                                                   interp_t,
                                                   joint_trans.rotation);
                                    break;

                                case fastgltf::AnimationPath::Scale:
                                    glm_vec3_lerp(const_cast<float_t*>(output0.raw),
                                                  const_cast<float_t*>(output1.raw),
                                                  interp_t,
                                                  joint_trans.scale);
                                    break;

                                case fastgltf::AnimationPath::Weights:
                                    // Not supported.
                                    assert(false);
                                    break;

                                default:
                                    // Huh?
                                    assert(false);
                                    return;
                            }

                            found_sample = true;
                            break;
                        }
                    assert(found_sample);
                }

                // Create animation frame from pose.
                Model_joint_animation_frame new_frame;
                new_frame.joint_transforms_in_order.reserve(
                    new_deformed_model_skin.joints_sorted_breadth_first.size());

                // @NOTE: This uses `i` the index of the model joint list (sorted breadth first)
                //   to access the local trans map (instead of contents of `node_index_insert_order`
                //   which is WRONG)  -Thea 2025/07/20
                for (size_t i = 0; i < new_deformed_model_skin.joints_sorted_breadth_first.size(); i++)
                {
                    new_frame.joint_transforms_in_order.emplace_back(
                        std::move(joint_idx_to_local_trans_map.at(i)));
                }

                // @NOTE: Just copying the root position and will do delta calculations later.
                glm_vec3_copy(root_pos, new_frame.root_motion_delta_pos);

                new_anim_frames.emplace_back(std::move(new_frame));

                // Tick next frame (and clamp to end time for any possible extra frames).
                curr_time = std::min(curr_time + k_anim_frame_time,
                                     end_time);
            }

            // Turn root positions (stored in `.root_motion_delta_pos`) into delta positions.
            for (size_t i = 1; i < new_anim_frames.size(); i++)
            {
                glm_vec3_sub(new_anim_frames[i - 0].root_motion_delta_pos,
                             new_anim_frames[i - 1].root_motion_delta_pos,
                             new_anim_frames[i - 1].root_motion_delta_pos);
            }
            {   // Special case for last frame (avg prev and last deltas (^_^;) ).
                assert(new_anim_frames.size() >= 2);
                vec3 avg_delta_pos = GLM_VEC3_ZERO_INIT;
                glm_vec3_muladds(new_anim_frames[new_anim_frames.size() - 2].root_motion_delta_pos,
                                 0.5f,
                                 avg_delta_pos);
                glm_vec3_muladds(new_anim_frames[0].root_motion_delta_pos,
                                 0.5f,
                                 avg_delta_pos);
                glm_vec3_copy(avg_delta_pos,
                              new_anim_frames[new_anim_frames.size() - 1].root_motion_delta_pos);
            }
        }

        // Create animation.
        m_animations.emplace_back(std::ref(new_deformed_model_skin),
                                  anim_name,
                                  std::move(new_anim_frames));
    }
}

}  // namespace TXP


TXP::Render_model TXP::load_model_from_disk(std::string const& model_name,
                                            std::string const& file_ext)
{
    if (file_ext == ".obj")
    {
        return load_obj_model_from_disk(s_model_directory + model_name + file_ext);
    }
    else if (file_ext == ".glb" || file_ext == ".gltf")
    {
        return load_gltf_model_from_disk(s_model_directory + model_name + file_ext);
    }
    else
    {
        throw std::runtime_error("Unknown model asset file extension: " + file_ext);
    }
}
