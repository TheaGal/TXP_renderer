#pragma once

#include "btglm.h"
#include "nlohmann/detail/macro_scope.hpp"

#include <cstdint>
#include <string>


namespace TXP
{

/// Frames per second all skeletal animations are imported as.
constexpr float_t k_skeletal_anim_frames_per_second{ 60.0f };

/// Tick interval for simulation thread.
constexpr float_t k_simulation_delta_time{ 1.0f / k_skeletal_anim_frames_per_second };

/// Key to access editing render objects.
using pool_key_t = std::uint32_t;

/// If pool key is set to this, the renderer will process this render object (effectively
/// creating/recreating a render object).
constexpr pool_key_t k_pool_key_process_flag{ 0 };

/// Bitmask for filtering layers to render.
enum Render_layer : uint16_t
{
    RENDER_LAYER_ALL          = 0b1111'1111'1111'1111,
    RENDER_LAYER_NONE         = 0b0000'0000'0000'0000,

    RENDER_LAYER_DEFAULT      = 0b0000'0000'0000'0001,
    RENDER_LAYER_INVISIBLE    = 0b0000'0000'0000'0010,
    RENDER_LAYER_LEVEL_EDITOR = 0b0000'0000'0000'0100,
};


namespace component
{

/// Config for a render object (to be used as ECS component).
struct Render_object_config
{
    Render_layer render_layer;
    std::string model_name;

    mat4s transform = mat4s{ GLM_MAT4_IDENTITY_INIT };

    // ^^ Required ^^ / vv Optional vv

    std::string material_palette;

    struct Animated_create_config
    {
        std::string animator_template;
        std::string anim_frame_action_control;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Animated_create_config,
                                       animator_template,
                                       anim_frame_action_control);
    } deform_config;

    // ^^ Optional ^^ / vv Set up by Renderer vv

    struct Renderer_owned_data
    {
        pool_key_t pool_key{ k_pool_key_process_flag };
    } renderer_owned_data;


    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Render_object_config,
                                   render_layer,
                                   model_name,
                                   transform,
                                   material_palette,
                                   deform_config);
};

}  // namespace component
}  // namespace TXP
