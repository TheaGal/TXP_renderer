#pragma once

#include "btglm.h"
#include "deformed_render_model.h"

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace TXP
{

struct Model_joint_animation_frame  // @TODO: rename to `Skeletal_animation_frame`.
{
    struct Joint_local_transform
    {
        vec3 position;
        versor rotation;
        vec3 scale;

#if 0  /* @NOTE: I really don't think I can handle step interpolation. It's too 細かい from gltf to use here. */
        // (FOR NOW OR MAYBE FOREVER) interpolation type is ignored and only linear is used
        // at least for skeletal animation.
        enum Interpolation_type
        {
            INTERP_TYPE_LINEAR = 0,
            INTERP_TYPE_STEP,
            NUM_INTERP_TYPES
        } interp_type;
#endif  // 0

        Joint_local_transform interpolate_fast(Joint_local_transform const& other,
                                               float_t t) const;
    };
    std::vector<Joint_local_transform> joint_transforms_in_order;

    vec3 root_motion_delta_pos;
};

class Model_joint_animation  // @TODO: rename to `Skeletal_animation`.
{
public:
    Model_joint_animation(Deformed_model_skin const& skin,
                          std::string name,
                          std::vector<Model_joint_animation_frame>&& animation_frames);

    std::string get_name() const { return m_name; }
    size_t get_num_frames() const { return m_frames.size(); }

    enum Rounding_func{ FLOOR, CEIL };
    uint32_t calc_frame_idx(float_t time, bool loop, Rounding_func rounding) const;

    using Joint_local_transform_set_t =
        std::vector<Model_joint_animation_frame::Joint_local_transform>;

    Joint_local_transform_set_t calc_joint_local_transforms_interpolated(
        float_t time,
        bool loop,
        bool root_motion_zeroing) const;
    Joint_local_transform_set_t calc_joint_local_transforms_floored(float_t time,
                                                                    bool loop,
                                                                    bool root_motion_zeroing) const;

    static Joint_local_transform_set_t blend_joint_local_transform_sets(
        Joint_local_transform_set_t const& a,
        Joint_local_transform_set_t const& b,
        float_t blend_t);

    // @TODO: @THEA: This func should get moved to `Model_animator` instead!!!!
    // @NOCHECKIN: Do ^^ above ^^
    void calc_joint_matrices(Joint_local_transform_set_t const& joint_local_transforms,
                             std::vector<mat4s>& out_joint_matrices) const;

    void get_root_motion_delta_pos_at_frame(uint32_t frame_idx,
                                            vec3& out_root_motion_delta_pos) const;

private:
    Deformed_model_skin const& m_model_skin;

    std::string m_name;
    std::vector<Model_joint_animation_frame> m_frames;
};

/// Holds a set of animations to be used by the animator.
struct Deformed_model_animation_set
{
    std::vector<Model_joint_animation> animations;
};

}  // namespace TXP
