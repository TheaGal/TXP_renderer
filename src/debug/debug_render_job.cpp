// clang-format off
#include "txp_renderer/debug/debug_render_job.h"
#include "debug_render_job_internal.h"
// clang-format on

#include "btdatecheck.h"
#include "btglm.h"
#include "entt/entity/registry.hpp"
#include "mutex_wrapper/mutex_wrapper.h"
#include "txp_renderer/types.h"

#include <set>
#include <string>
#include <unordered_map>


namespace TXP
{
namespace
{

using namespace debug;

static std::unordered_map<Material_type, char const*> const k_material_type_str_map{
    { PHYSICS_WIREFRAME, "__debug_color_wireframe_physics_mesh_mat_pal" },
    { SELECTED_WIREFRAME, "__debug_color_wireframe_selected_mesh_mat_pal" },
};

entt::registry* g_reg{ nullptr };

std::function<entt::entity()> g_create_ecs_entity_fn{ nullptr };
std::function<void(entt::entity)> g_destroy_ecs_entity_fn{ nullptr };

BT::Mutex_wrapper<std::unordered_map<debug_model_id_t, entt::entity>> g_debug_models;


struct Debug_line_info_collection
{
    std::set<uint32_t> expired_indexes;
    std::vector<Debug_line> dbg_lines;
    std::vector<float_t> dbg_lines_timeouts;
};

BT::Mutex_wrapper<Debug_line_info_collection> g_dbg_line_info_coll;

} // namespace


void debug::set_callbacks_and_references(
    entt::registry& reg,
    std::function<entt::entity()>&& create_ecs_entity_callback,
    std::function<void(entt::entity)>&& destroy_ecs_entity_callback)
{
    g_reg = &reg;
    g_create_ecs_entity_fn = create_ecs_entity_callback;
    g_destroy_ecs_entity_fn = destroy_ecs_entity_callback;
}


debug::debug_model_id_t debug::emplace_debug_model(std::string const& model_name,
                                                   Material_type material)
{
    auto debug_models{ g_debug_models.scoped_lock() };

    debug_model_id_t new_id{ 0 };
    while (debug_models->find(new_id) != debug_models->end())
    {
        new_id++;
    }

    entt::entity ent = debug_models->emplace(new_id, g_create_ecs_entity_fn()).first->second;

    auto& rend_obj_cfg = g_reg->emplace<component::Render_object_config>(ent);
    rend_obj_cfg.render_layer = RENDER_LAYER_DEFAULT;
    rend_obj_cfg.model_name = model_name;
    rend_obj_cfg.material_palette = k_material_type_str_map.at(material);

    return new_id;
}

void debug::remove_debug_model(debug_model_id_t model_id)
{
    auto debug_models{ g_debug_models.scoped_lock() };

    g_destroy_ecs_entity_fn(debug_models->at(model_id));
    debug_models->erase(model_id);
}

void debug::update_debug_model_transform(debug_model_id_t model_id, mat4 transform)
{
    auto debug_models{ g_debug_models.scoped_lock() };

    auto& rend_obj_cfg = g_reg->get<component::Render_object_config>(debug_models->at(model_id));
    glm_mat4_copy(transform, rend_obj_cfg.transform.raw);
}


namespace
{

void emplace_debug_line_internal(Debug_line_info_collection& dbg_line_info_coll,
                                 Debug_line&& line,
                                 float_t timeout)
{
    if (dbg_line_info_coll.expired_indexes.empty())
    {
        // Add new entry.
        dbg_line_info_coll.dbg_lines.emplace_back(std::move(line));
        dbg_line_info_coll.dbg_lines_timeouts.emplace_back(timeout);
    }
    else
    {
        // Erase first expired index.
        auto it{ dbg_line_info_coll.expired_indexes.begin() };
        uint32_t expired_idx{ *it };
        dbg_line_info_coll.expired_indexes.erase(it);

        // Replace that entry.
        dbg_line_info_coll.dbg_lines[expired_idx] = std::move(line);
        dbg_line_info_coll.dbg_lines_timeouts[expired_idx] = timeout;
    }
}

} // namespace

void debug::emplace_debug_line(Debug_line&& line, float_t timeout)
{
    emplace_debug_line_internal(*g_dbg_line_info_coll.scoped_lock(), std::move(line), timeout);
}

void debug::emplace_debug_line_based_capsule(vec3 origin_a,
                                             vec3 origin_b,
                                             float_t radius,
                                             vec4 color,
                                             float_t timeout)
{   // Calculate basis vectors.
    vec3s basis_x{ 1.0f, 0.0f, 0.0f };
    vec3s basis_y{ 0.0f, 1.0f, 0.0f };
    vec3s basis_z{ 0.0f, 0.0f, 1.0f };

    if (glm_vec3_distance2(origin_a, origin_b) > 1e-6f)
    {   // Calc basis-y.
        glm_vec3_sub(origin_b, origin_a, basis_y.raw);
        glm_vec3_normalize(basis_y.raw);

        // Calc next basis axis.
        bool using_z_axis{ false };
        vec3s some_axis{ 0.0f, 1.0f, 0.0f };
        if (std::abs(basis_y.y) > 1.0f - 1e-6f)
        {
            using_z_axis = true;
            some_axis = { 0.0f, 0.0f, 1.0f };
        }

        glm_vec3_crossn(basis_y.raw, some_axis.raw, basis_x.raw);

        // Calc final basis axis.
        glm_vec3_crossn(basis_x.raw, basis_y.raw, basis_z.raw);
    }

    // Transform points.
    static std::vector<vec3s> const k_end_cap_a_x{
        { 0,  0,          -1          },
        { 0, -0.38268343, -0.92387953 },
        { 0, -0.70710678, -0.70710678 },
        { 0, -0.92387953, -0.38268343 },
        { 0, -1,           0          },
        { 0, -0.92387953,  0.38268343 },
        { 0, -0.70710678,  0.70710678 },
        { 0, -0.38268343,  0.92387953 },
        { 0,  0,           1          },
    };
    static std::vector<vec3s> const k_end_cap_a_z{
        { -1,           0,          0 },
        { -0.92387953, -0.38268343, 0 },
        { -0.70710678, -0.70710678, 0 },
        { -0.38268343, -0.92387953, 0 },
        {  0,          -1,          0 },
        {  0.38268343, -0.92387953, 0 },
        {  0.70710678, -0.70710678, 0 },
        {  0.92387953, -0.38268343, 0 },
        {  1,           0,          0 },
    };
    static std::vector<vec3s> const k_end_cap_b_x{
        { 0, 0,           1          },
        { 0, 0.38268343,  0.92387953 },
        { 0, 0.70710678,  0.70710678 },
        { 0, 0.92387953,  0.38268343 },
        { 0, 1,           0          },
        { 0, 0.92387953, -0.38268343 },
        { 0, 0.70710678, -0.70710678 },
        { 0, 0.38268343, -0.92387953 },
        { 0, 0,          -1          },
    };
    static std::vector<vec3s> const k_end_cap_b_z{
        {  1,           0,          0 },
        {  0.92387953,  0.38268343, 0 },
        {  0.70710678,  0.70710678, 0 },
        {  0.38268343,  0.92387953, 0 },
        {  0,           1,          0 },
        { -0.38268343,  0.92387953, 0 },
        { -0.70710678,  0.70710678, 0 },
        { -0.92387953,  0.38268343, 0 },
        { -1,           0,          0 },
    };
    static std::vector<vec3s> const k_end_cap_ab_y{
        {  1,          0,  0          },
        {  0.92387953, 0,  0.38268343 },
        {  0.70710678, 0,  0.70710678 },
        {  0.38268343, 0,  0.92387953 },
        {  0,          0,  1          },
        { -0.38268343, 0,  0.92387953 },
        { -0.70710678, 0,  0.70710678 },
        { -0.92387953, 0,  0.38268343 },
        { -1,          0,  0          },
        { -0.92387953, 0, -0.38268343 },
        { -0.70710678, 0, -0.70710678 },
        { -0.38268343, 0, -0.92387953 },
        {  0,          0, -1          },
        {  0.38268343, 0, -0.92387953 },
        {  0.70710678, 0, -0.70710678 },
        {  0.92387953, 0, -0.38268343 },
        {  1,          0,  0          },
    };

    struct Transformed_end_cap_point_set
    {
        std::vector<vec3s> const& base_end_cap_ref;
        bool use_origin_a;
        std::vector<vec3s> trans_end_cap;
    };
    std::vector<Transformed_end_cap_point_set> trans_ecp_sets{
        { k_end_cap_a_x, true },    // End cap A-X.
        { k_end_cap_a_z, true },    // End cap A-Z.
        { k_end_cap_ab_y, true },   // End cap A-Y.
        { k_end_cap_b_x, false },   // End cap B-X.
        { k_end_cap_b_z, false },   // End cap B-Z.
        { k_end_cap_ab_y, false },  // End cap B-Y.
    };

    for (auto& ecp_set : trans_ecp_sets)
    {
        ecp_set.trans_end_cap.reserve(ecp_set.base_end_cap_ref.size());

        for (auto& pt : ecp_set.base_end_cap_ref)
        {   // Transform point into capsule transform.
            vec3s trans_pt;
            glm_vec3_scale(basis_x.raw, pt.x * radius, trans_pt.raw);
            glm_vec3_muladds(basis_y.raw, pt.y * radius, trans_pt.raw);
            glm_vec3_muladds(basis_z.raw, pt.z * radius, trans_pt.raw);

            glm_vec3_add(trans_pt.raw,
                         ecp_set.use_origin_a ? origin_a : origin_b,
                         trans_pt.raw);

            ecp_set.trans_end_cap.emplace_back(trans_pt);
        }
    }

    // Emplace points as lines.
    auto dbg_line_info_coll{ g_dbg_line_info_coll.scoped_lock() };

    for (auto& ecp_set : trans_ecp_sets)
        for (size_t idx = 1; idx < ecp_set.trans_end_cap.size(); idx++)
        {   // Draw line from translated end cap points.
            Debug_line new_line;
            glm_vec3_copy(ecp_set.trans_end_cap[idx - 1].raw, new_line.pos1);
            glm_vec3_copy(ecp_set.trans_end_cap[idx + 0].raw, new_line.pos2);
            glm_vec4_copy(color, new_line.color1);
            glm_vec4_copy(color, new_line.color2);

            emplace_debug_line_internal(*dbg_line_info_coll, std::move(new_line), timeout);
        }

    auto& end_cap_a_y{ trans_ecp_sets[2] };
    auto& end_cap_b_y{ trans_ecp_sets[5] };
    for (size_t idx = 0; idx < 16; idx += 2)
    {   // Draw "ribs": connecting lines between end caps.
        Debug_line new_line;
        glm_vec3_copy(end_cap_a_y.trans_end_cap[idx].raw, new_line.pos1);
        glm_vec3_copy(end_cap_b_y.trans_end_cap[idx].raw, new_line.pos2);
        glm_vec4_copy(color, new_line.color1);
        glm_vec4_copy(color, new_line.color2);

        emplace_debug_line_internal(*dbg_line_info_coll, std::move(new_line), timeout);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Internal functions.

void debug::tick_debug_render_jobs(float_t delta_time)
{
    assert(false);
}

size_t calc_debug_line_mem_size()
{
    assert(false);
}

void write_debug_line_mem(void* dest)
{
    assert(false);
}

} // namespace TXP
