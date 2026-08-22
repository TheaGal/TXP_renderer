#include "txp_renderer/debug/debug_render_job.h"

#include "btdatecheck.h"
#include "btglm.h"
#include "entt/entity/registry.hpp"
#include "mutex_wrapper/mutex_wrapper.h"
#include "txp_renderer/types.h"

#include <string>
#include <unordered_map>


namespace TXP
{
namespace
{

using namespace debug;

static std::unordered_map<Material_type, char*> const k_material_type_str_map{

};

entt::registry* g_reg{ nullptr };
BT::Mutex_wrapper<std::unordered_map<debug_model_id_t, entt::entity>> g_debug_models;

} // namespace


void debug::set_ecs_registry_reference(entt::registry& reg)
{
    g_reg = &reg;
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

    entt::entity ent = debug_models->emplace(new_id, g_reg->create()).first->second;

    auto& rend_obj_cfg = g_reg->emplace<component::Render_object_config>(ent);
    rend_obj_cfg.render_layer = RENDER_LAYER_DEFAULT;
    rend_obj_cfg.model_name = model_name;
    rend_obj_cfg.material_palette = k_material_type_str_map.at(material);

    return 0;
}

void debug::remove_debug_model(debug_model_id_t model_id)
{
    auto debug_models{ g_debug_models.scoped_lock() };

    debug_models->erase(model_id);
}

void debug::update_debug_model_transform(debug_model_id_t model_id, mat4 transform)
{
    auto debug_models{ g_debug_models.scoped_lock() };

    auto& rend_obj_cfg = g_reg->get<component::Render_object_config>(debug_models->at(model_id));
    glm_mat4_copy(transform, rend_obj_cfg.transform.raw);
}

void debug::emplace_debug_line(Debug_line&& line, float_t duration)
{
    BT::date_deadline(2026, 8, 25);
}

void debug::emplace_debug_line_based_capsule(vec3 origin_a,
                                             vec3 origin_b,
                                             float_t radius,
                                             vec4 color,
                                             float_t timeout)
{
    BT::date_deadline(2026, 8, 25);
}

} // namespace TXP
