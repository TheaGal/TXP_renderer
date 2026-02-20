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
    Render_layer layer{ RENDER_LAYER_DEFAULT };

    uint16_t render_model_idx;
    uint16_t material_set_idx;  // Default: pulls from render model material set.
    uint16_t animator_idx{ (uint16_t)-1 };  // opt: -1 means no animator.

    size_t padding0;

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

    // @TODO: @CHECK: @THEA: change emplace funcs to void?
    uint16_t emplace_static_model_data_set(std::string const& name, Static_model_data_set&& data);
    uint16_t get_static_model_data_set(std::string const& name);

    uint16_t emplace_deformed_model_skin(std::string const& name, Deformed_model_skin&& data);
    uint16_t get_deformed_model_skin(std::string const& name);

    uint16_t emplace_deformed_model_anim_set(std::string const& name, Deformed_model_animation_set&& data);
    uint16_t get_deformed_model_anim_set(std::string const& name);

    uint16_t emplace_deformed_vertex_buffer(std::string const& name, void* data);
    uint16_t get_deformed_vertex_buffer(std::string const& name);

    uint16_t emplace_material_set(std::string const& name, Material_set&& data);
    uint16_t get_material_set(std::string const& name);

    // Pimpl.
    struct Data;
    std::unique_ptr<Data> data;
};

}  // namespace TXP
