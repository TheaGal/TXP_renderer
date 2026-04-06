#include "animator_template.h"

// #include "../btzc_game_engine.h"
#include "animator_template_types.h"
#include "btjson.h"
#include "btservice_finder.h"
#include "skeletal_animator.h"

#include <cassert>
#include <iterator>
#include <sstream>
#include <string>


TXP::Animator_template_bank::Animator_template_bank(std::string const& animator_template_asset_dir)
    : m_animator_template_asset_dir(animator_template_asset_dir)
{
    // Add self as service.
    BT_SERVICE_FINDER_ADD_SERVICE(Animator_template_bank, this);
}

TXP::Animator_template const& TXP::Animator_template_bank::load_animator_template(
    std::string const& anim_template_name)
{
    if (m_anim_template_cache.find(anim_template_name) == m_anim_template_cache.end())
    {   // Load from disk.
        json root = BT::json_load_from_disk(m_animator_template_asset_dir + anim_template_name +
                                            ".btanitor");

        // Fill in new struct.
        Animator_template new_template = root;

        // Cook animator template variables.
        new_template.variables_cooked.resize(new_template.variables.size());
        for (size_t i = 0; i < new_template.variables.size(); i++)
        {   // Get tokens of variable string.
            std::vector<std::string> tokens;
            {
                std::istringstream iss(new_template.variables[i]);
                std::copy(std::istream_iterator<std::string>(iss),
                          std::istream_iterator<std::string>(),
                          std::back_inserter(tokens));
                assert(tokens.size() == 2);
            }

            // Start cookin'
            auto& ckd{ new_template.variables_cooked[i] };

            // Convert type.
            if      (tokens[0] == "bool")  ckd.type = anim_tmpl_types::Animator_variable::TYPE_BOOL;
            else if (tokens[0] == "int")   ckd.type = anim_tmpl_types::Animator_variable::TYPE_INT;
            else if (tokens[0] == "float") ckd.type = anim_tmpl_types::Animator_variable::TYPE_FLOAT;
            else if (tokens[0] == "trig")  ckd.type = anim_tmpl_types::Animator_variable::TYPE_TRIGGER;
            else assert(false);

            // Convert name.
            ckd.var_name = tokens[1];
        }

        m_anim_template_cache.emplace(anim_template_name, std::move(new_template));
    }


    // Grab template from cache.
    return m_anim_template_cache.at(anim_template_name);
}

void TXP::Animator_template_bank::load_animator_template_into_animator(
    component_internal::Model_animator& animator,
    std::string const& anim_template_name)
{
    auto const& anim_temp{ load_animator_template(anim_template_name) };

    // Write to model animator.
    std::vector<anim_tmpl_types::Animator_state> anim_states;
    anim_states.reserve(anim_temp.animator_states.size());
    for (auto const& temp_anim_state : anim_temp.animator_states)
    {
        auto state_type{ temp_anim_state.state_type == "single_anim"
                             ? anim_tmpl_types::Animator_state::SINGLE_ANIM
                             : anim_tmpl_types::Animator_state::BLENDTREE };

        std::vector<anim_tmpl_types::Animator_state::Blend_anim> blend_anims;
        if (state_type == anim_tmpl_types::Animator_state::BLENDTREE)
        {
            for (auto const& blend_anim : temp_anim_state.blend_anims)
                blend_anims.emplace_back(animator.get_model_animation_idx(blend_anim.anim_name),
                                         blend_anim.value);
        }

        anim_states.emplace_back(temp_anim_state.state_name,
                                 state_type,
                                 state_type == anim_tmpl_types::Animator_state::SINGLE_ANIM
                                     ? animator.get_model_animation_idx(temp_anim_state.anim_name)
                                     : (uint32_t)-1,
                                 temp_anim_state.blend_var,
                                 std::move(blend_anims));
    }

    // @TODO: Also include transition states in model animator.

    animator.configure_animator_states(anim_states,
                                       anim_temp.variables_cooked);
}
