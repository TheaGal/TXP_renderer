#pragma once

#include "animator_template_types.h"

#include "btjson.h"

#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


namespace TXP
{

namespace component_internal
{
class Model_animator;
}  // namespace component_internal

struct Animator_template
{
    struct Animator_state
    {
        std::string state_name{ "INVALID_STATE_NAME" };
        std::string state_type{ "single_anim" };  // [single_anim, blendtree]

        // Single anim.
        std::string anim_name{ "INVALID_ANIM_NAME" };

        // Blendtree.
        std::string blend_var{ "INVALID_BLEND_VAR_NAME" };
        struct Blend_anim
        {
            std::string anim_name;
            float_t value;

            NLOHMANN_DEFINE_TYPE_INTRUSIVE(Blend_anim,
                                           anim_name,
                                           value);
        };
        std::vector<Blend_anim> blend_anims;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Animator_state,
                                                    state_name,
                                                    state_type,
                                                    anim_name,
                                                    blend_var,
                                                    blend_anims);
    };
    std::vector<Animator_state> animator_states;

    /// Variables to be used in state transition conditions.
    std::vector<std::string> variables;

    /// DO NOT INCLUDE IN SERIALIZATION.
    std::vector<anim_tmpl_types::Animator_variable> variables_cooked;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Animator_template,
                                   animator_states,
                                   variables);
};

class Animator_template_bank
{
public:
    Animator_template_bank(std::string const& animator_template_asset_dir);

    Animator_template const& load_animator_template(std::string const& anim_template_name);
    void load_animator_template_into_animator(component_internal::Model_animator& animator,
                                              std::string const& anim_template_name);

private:
    std::string m_animator_template_asset_dir;
    std::unordered_map<std::string, Animator_template> m_anim_template_cache;
};

}  // namespace TXP
