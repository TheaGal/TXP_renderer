#pragma once

#include "btglm.h"
#include "btjson.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>


namespace TXP
{
namespace UI
{

constexpr float_t k_ui_canvas_fixed_height{ 360 };

struct Rect_transform
{
    float_t x;
    float_t y;
    float_t w;
    float_t h;

    float_t rot;

    struct Anchor
    {
        float_t x;
        float_t y;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Anchor, x, y);
    } anchor;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Rect_transform, x, y, w, h, rot, anchor);
};

struct UI_file_element_data
{
    std::string name;
    std::string parent;
    Rect_transform transform;
    float_t opacity;
    std::string image;
    vec3s color_tint;
    bool is_nine_slice;
    std::string render_text;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UI_file_element_data,
                                   name,
                                   parent,
                                   transform,
                                   opacity,
                                   image,
                                   color_tint,
                                   is_nine_slice,
                                   render_text);
};

struct UI_file_data
{
    std::vector<UI_file_element_data> elements;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UI_file_data, elements);
};

struct UI_element
{
    std::string name;
    UI_element* parent{ nullptr };
    Rect_transform transform;  // local transform.
    float_t opacity;
    uint32_t texture_idx;
    vec3 color_tint{ 1, 1, 1 };
    bool is_nine_slice;
    std::string render_text;
};

} // namespace UI
} // namespace TXP
