#pragma once

#include "shader_creation/shader_creation.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


namespace TXP
{

/// Frames per second all skeletal animations are imported as.
constexpr float_t k_skeletal_anim_frames_per_second{ 60.0f };

/// Tick interval for simulation thread.
constexpr float_t k_simulation_delta_time{ 1.0f / k_skeletal_anim_frames_per_second };

/// Information to create texture asset.
struct Texture_asset_create_info
{
    std::string texture_name;
    std::string ktx2_fname;
};

/// Information to create material asset.
struct Material_asset_create_info
{
    std::string material_name;
    std::pair<std::string, Shader_Creation::Shader_pipeline_type> shader_name_and_type;
    std::unordered_map<std::string, std::string> shader_params;
};

/// Information to create material-set asset.
struct Material_set_asset_create_info
{
    std::string mat_set_name;
    std::vector<std::string> materials;
};

/// Information to create model asset.
struct Model_asset_create_info
{
    std::string model_name;
    std::string file_ext;
};

/// Key to access editing render objects.
using pool_key_t = std::uint32_t;

/// Bitmask for filtering layers to render.
enum Render_layer : uint16_t
{
    RENDER_LAYER_ALL          = 0b1111'1111'1111'1111,
    RENDER_LAYER_NONE         = 0b0000'0000'0000'0000,

    RENDER_LAYER_DEFAULT      = 0b0000'0000'0000'0001,
    RENDER_LAYER_INVISIBLE    = 0b0000'0000'0000'0010,
    RENDER_LAYER_LEVEL_EDITOR = 0b0000'0000'0000'0100,
};

/// Config for creating a render object.
struct Render_obj_create_config
{
    Render_layer layer;
    std::string model_name;

    // ^^ Required ^^ / vv Optional vv

    std::string material_set;

    struct Animated_create_config
    {
        std::string animator_template;
        std::string anim_frame_action_control;
    } deform_config;
};

}  // namespace TXP
