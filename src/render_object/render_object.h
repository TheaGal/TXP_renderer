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
    uint16_t material_palette_idx;  // Default: pulls from render model material palette.
    uint16_t animator_idx{ (uint16_t)-1 };  // opt: -1 means no animator.

    uint16_t padding1;
    uint32_t padding2;

    mat4 transform = GLM_MAT4_IDENTITY_INIT;

    bool is_animated() const
    {
        return (animator_idx != (uint16_t)-1);
    }
};

/// Holds reference for a render object's model mesh.
/// For keeping track of model's mesh when different materials are used per mesh.
struct Render_object_model_mesh_reference
{
    uint16_t render_obj_idx;
    uint16_t model_mesh_idx;
};

// Forward declarations for collection.
struct Static_model_data_set;
struct Deformed_model_data_set;
struct Deformed_model_skin;
struct Deformed_model_animation_set;
struct Material_palette;

/// Collection that holds pools of render data.  @TODO: move this to its own file.
struct Render_model_data_collection
{
    Render_model_data_collection();
    ~Render_model_data_collection();

    void emplace_static_model_data_set(std::string const& name, Static_model_data_set&& data);
    std::vector<std::string> get_static_model_data_set_name_list() const;
    uint16_t get_static_model_data_set_idx(std::string const& name) const;
    Static_model_data_set const& get_static_model_data_set(uint16_t idx) const;

    void lock_in_number_of_static_models();

    void emplace_deformed_model_skin(std::string const& name, Deformed_model_skin&& data);
    std::vector<std::string> get_deformed_model_skin_name_list() const;
    uint16_t get_deformed_model_skin_idx(std::string const& name) const;
    Deformed_model_skin const& get_deformed_model_skin(uint16_t idx) const;

    void emplace_deformed_model_anim_set(std::string const& name, Deformed_model_animation_set&& data);
    std::vector<std::string> get_deformed_model_anim_set_name_list() const;
    uint16_t get_deformed_model_anim_set_idx(std::string const& name) const;
    Deformed_model_animation_set const& get_deformed_model_anim_set(uint16_t idx) const;

    uint16_t create_deformed_model_from_static_model_data_set(std::string const& static_model_name);
    bool is_static_model_idx(uint16_t render_model_idx) const;
    uint16_t translate_to_static_model_data_set_idx(uint16_t render_model_idx) const;
    std::vector<Deformed_model_data_set*> get_all_deformed_models();
    Deformed_model_data_set const& get_deformed_model_data_set(uint16_t render_model_idx) const;

    /// Adds a user to the reference/usage counter of a certain model.
    /// @param render_model_idx index of either static model or deformed model.
    void report_one_user_added(uint16_t render_model_idx);

    /// Removes a user from the reference/usage counter of a certain model.
    /// @param render_model_idx index of either static model or deformed model.
    void report_one_user_removed(uint16_t render_model_idx, bool& out_now_unused);

    // Pimpl.
    struct Data;
    std::unique_ptr<Data> inner_data;
};

}  // namespace TXP
