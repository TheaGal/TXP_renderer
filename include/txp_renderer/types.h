#pragma once

#include "btglm.h"
#include "btjson.h"
#include "btuuid.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>


namespace TXP
{

/// Performance time types for reporting performance.
enum Performance_time_type : uint32_t
{
    PERF_TIME_TYPE_SIMULATION_LOOP = 0,
    PERF_TIME_TYPE_RENDERER_LOOP,

    NUM_PERF_TIME_TYPES
};

/// Labels for performance time types.
constexpr std::array<char const* const, NUM_PERF_TIME_TYPES> k_performance_time_type_labels{
    "Simulation Loop",
    "Renderer Loop",
};

/// Collects samples, overwriting old ones.
class Rolling_sampler
{
public:
    Rolling_sampler()
    {
        for (auto& s : m_samples)
            s = -1;  // Init w invalid samples
    }

    void add_sample(float_t sample)
    {
        assert(sample >= 0);

        auto cur = (m_cursor++) % m_samples.size();
        m_samples[cur] = sample;
    }

    std::vector<float_t> get_samples() const
    {
        std::vector<float_t> valid_samples;

        auto cur{ m_cursor };
        auto end_pos{ cur % m_samples.size() };

        while (true)
        {
            cur = (cur + 1) % m_samples.size();

            if (auto s = m_samples[cur]; s >= 0)
            {
                valid_samples.emplace_back(s);
            }

            if (cur == end_pos)
                break;
        }

        return valid_samples;
    }

private:
    uint16_t m_cursor{ 0 };
    std::array<float_t, 512> m_samples;  // Ensure size is power of 2.
};

/// Type for storing performance times.
/// @NOTE: this doesn't have to be public.
using Performance_time_map_t = std::unordered_map<Performance_time_type, Rolling_sampler>;

/// Frames per second all skeletal animations are imported as.
constexpr float_t k_skeletal_anim_frames_per_second{ 60.0f };

/// Tick interval for simulation thread.
constexpr float_t k_simulation_delta_time{ 1.0f / k_skeletal_anim_frames_per_second };

/// Key to access editing render objects.
using pool_key_t = std::uint32_t;

/// If pool key is set to this, the renderer will process this render object (effectively
/// creating/recreating a render object).
constexpr pool_key_t k_pool_key_process_flag{ (pool_key_t)-1 };

/// Bitmask for filtering layers to render.
enum Render_layer : uint16_t
{
    RENDER_LAYER_ALL          = 0b1111'1111'1111'1111,
    RENDER_LAYER_NONE         = 0b0000'0000'0000'0000,

    RENDER_LAYER_DEFAULT      = 0b0000'0000'0000'0001,
    RENDER_LAYER_INVISIBLE    = 0b0000'0000'0000'0010,
    RENDER_LAYER_LEVEL_EDITOR = 0b0000'0000'0000'0100,
};


/// Profile enum for which timing of the animator to base calculations off of.
enum Animator_timer_profile
{
    SIMULATION_TIMER_PROFILE,
    RENDERER_TIMER_PROFILE,
};

/// State set. Once an animation state finishes, the animator changes to the next state in the
/// `anim_state_indices` list. Once the final state finishes, it will either stop, or loop
/// depending on `loop_final_state`.
struct Animator_state_set
{
    std::vector<uint32_t> anim_state_indices;
    bool loop_final_state;
};


namespace component
{

/// Metadata of an entity.
struct Entity_metadata
{
    std::string name;
    BT::UUID uuid;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        Entity_metadata,
        name,
        uuid
    );
};

/// Config for a render object (to be used as ECS component).
struct Render_object_config
{
    Render_layer render_layer{ 0 };
    std::string model_name;

    mat4s transform = mat4s{ GLM_MAT4_IDENTITY_INIT };

    // ^^ Required ^^ / vv Optional vv

    std::string sub_mesh_name;
    bool sub_mesh_zero_origin_position;  // @NOTE: setting this to true will crash the program since implementation is aborted.  -Thea 2026/08/04

    std::string material_palette;
    bool is_deformed{ false };

    // ^^ Optional ^^ / vv Set up by Renderer vv

    struct Renderer_owned_data
    {
        pool_key_t pool_key{ k_pool_key_process_flag };
    } renderer_owned_data;


    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Render_object_config,
                                                render_layer,
                                                model_name,
                                                transform,
                                                material_palette,
                                                is_deformed);
};

/// Component to store data from animator and AFA data for root motion.
struct Animator_root_motion
{   // Settings and also captured.
    float_t root_motion_multiplier{ 1.0f };  // @TODO: @THINK: Should this automatically set the AFA data when initialized that?

    // Captured values.
    vec3 delta_pos = GLM_VEC3_ZERO_INIT;
    float_t turn_speed{ 0 };
    bool can_do_turnaround_anim{ false };
    bool inherit_prev_velocity{ false };

    struct Mvt_input
    {
        bool enabled{ false };
        float_t max_speed{ 0 };
        float_t accel{ 0 };
        float_t decel{ 0 };
    } mvt_input;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Animator_root_motion, root_motion_multiplier);
};

/// Tag component to be used in the hitcapsule update function.
struct Animator_driven_hitcapsule_set
{
};

}  // namespace component
}  // namespace TXP
