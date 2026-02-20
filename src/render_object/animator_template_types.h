#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>


namespace TXP
{
namespace anim_tmpl_types
{

struct Animator_state
{
    std::string state_name;
    enum
    {
        SINGLE_ANIM,
        BLENDTREE
    } state_type;

    uint32_t animation_idx{ (uint32_t)-1 };

    std::string blend_var;

    struct Blend_anim
    {
        uint32_t animation_idx;
        float_t value;
    };
    std::vector<Blend_anim> blend_anims;
};

struct Animator_variable
{
    enum Type : int32_t
    {
        TYPE_INVALID = -1,
        TYPE_BOOL,
        TYPE_INT,
        TYPE_FLOAT,
        TYPE_TRIGGER
    } type;

    std::string var_name;

    float_t var_value{ 0 };
};

/// Special condition var indexes.
static constexpr size_t k_on_anim_end_var_idx{ (size_t)-2 };

/// Var values for special types.
static constexpr float_t k_bool_false     = 0;
static constexpr float_t k_bool_true      = 1;
static constexpr float_t k_trig_triggered = 1;

}  // namespace animator_template_types
}  // namespace TXP
