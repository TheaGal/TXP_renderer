#pragma once

#include "reflection_structs.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{
namespace Shader_Creation
{

/// Sets the asset directory for shaders.
void set_shader_directory(std::string const& dir_path);

Reflection::Shader_reflection read_slang_reflection(std::string const& shader_name);

std::string get_shader_module_path(std::string const& shader_name);


// vv OLD vv ///////////////////////////////////////////////////////////////////////////////////////


void clear_slang_reflection_collection();

/// Loads slang reflection data for a shader.
/// @param is_compute_shader if true, will look for a vertex stage and fragment stage.
///                          if false, will look for a compute stage.
void load_slang_reflection_into_collection(std::string const& shader_name);

enum Shader_pipeline_type
{
    SHAD_PIPE_TYPE_COMPUTE = 0,
    SHAD_PIPE_TYPE_VERTEX_FRAGMENT,
};

std::vector<std::string> get_shader_pipeline_stage_names(Shader_pipeline_type shader_type);

struct Extracted_info
{
    enum Descriptor_type
    {
        TXP_SC_DESC_TYPE_SAMPLER = 0,
        TXP_SC_DESC_TYPE_COMBINED_IMAGE_SAMPLER = 1,
        TXP_SC_DESC_TYPE_SAMPLED_IMAGE = 2,
        TXP_SC_DESC_TYPE_STORAGE_IMAGE = 3,
        TXP_SC_DESC_TYPE_UNIFORM_TEXEL_BUFFER = 4,
        TXP_SC_DESC_TYPE_STORAGE_TEXEL_BUFFER = 5,
        TXP_SC_DESC_TYPE_UNIFORM_BUFFER = 6,
        TXP_SC_DESC_TYPE_STORAGE_BUFFER = 7,
        TXP_SC_DESC_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
        TXP_SC_DESC_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
        TXP_SC_DESC_TYPE_INPUT_ATTACHMENT = 10,
        TXP_SC_DESC_TYPE_INLINE_UNIFORM_BLOCK = 1000138000,
        TXP_SC_DESC_TYPE_ACCELERATION_STRUCTURE_KHR = 1000150000,
        TXP_SC_DESC_TYPE_ACCELERATION_STRUCTURE_NV = 1000165000,
        TXP_SC_DESC_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM = 1000440000,
        TXP_SC_DESC_TYPE_BLOCK_MATCH_IMAGE_QCOM = 1000440001,
        TXP_SC_DESC_TYPE_TENSOR_ARM = 1000460000,
        TXP_SC_DESC_TYPE_MUTABLE_EXT = 1000351000,
        TXP_SC_DESC_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV = 1000570000,
    };

    struct Descriptor_binding
    {
        uint32_t binding_idx;
        Descriptor_type desc_type;
    };

    struct Descriptor_layout_info
    {
        std::unordered_map<std::string, Descriptor_binding> shader_param_to_layout_binding;
    };

    struct Entry_point
    {
        std::string entry_point_name;
        std::string entry_point_stage;
        std::array<uint32_t, 3> compute_thread_group_size;  // @NOTE: only valid in compute pipelines.
        Descriptor_layout_info desc_layout_info;
    };

    std::vector<Entry_point> entry_points;
};

Extracted_info extract_stuff(std::string const& shader_name, Shader_pipeline_type shader_type);

}  // namespace Shader_Creation
}  // namespace TXP
