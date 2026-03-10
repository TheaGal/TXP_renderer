#pragma once

#include "btglm.h"
#include "txp_renderer/types.h"

#include <cstddef>
#include <cstdint>


namespace TXP
{

/// Lightweight object with properties on what to render in the render-object stage.
struct Render_object
{
    bool is_stale{ false };

    bool padding0;

    Render_layer layer{ RENDER_LAYER_DEFAULT };

    uint16_t render_model_idx;
    uint16_t material_set_idx;  // Default: pulls from render model material set.
    uint16_t animator_idx{ (uint16_t)-1 };  // opt: -1 means no animator.

    uint16_t padding1;
    uint32_t padding2;

    mat4 transform = GLM_MAT4_IDENTITY_INIT;
};

// Forward declarations for collection.
struct Static_model_data_set;
struct Deformed_model_skin;
struct Deformed_model_animation_set;
struct Material_set;

/// Collection that holds pools of render data.
struct Render_model_data_collection
{
    Render_model_data_collection();
    ~Render_model_data_collection();

    void emplace_static_model_data_set(std::string const& name, Static_model_data_set&& data);
    std::vector<std::string> get_static_model_data_set_name_list() const;
    uint16_t get_static_model_data_set_idx(std::string const& name) const;
    Static_model_data_set const& get_static_model_data_set(uint16_t idx) const;

    void emplace_deformed_model_skin(std::string const& name, Deformed_model_skin&& data);
    std::vector<std::string> get_deformed_model_skin_name_list() const;
    uint16_t get_deformed_model_skin_idx(std::string const& name) const;
    Deformed_model_skin const& get_deformed_model_skin(uint16_t idx) const;

    void emplace_deformed_model_anim_set(std::string const& name, Deformed_model_animation_set&& data);
    std::vector<std::string> get_deformed_model_anim_set_name_list() const;
    uint16_t get_deformed_model_anim_set_idx(std::string const& name) const;
    Deformed_model_animation_set const& get_deformed_model_anim_set(uint16_t idx) const;

    uint16_t emplace_deformed_vertex_buffer(std::string const& name, void* data);
    uint16_t get_deformed_vertex_buffer(std::string const& name);

    uint16_t emplace_material_set(std::string const& name, Material_set&& data);
    uint16_t get_material_set(std::string const& name);

    // Pimpl.
    struct Data;
    std::unique_ptr<Data> inner_data;
};

}  // namespace TXP
