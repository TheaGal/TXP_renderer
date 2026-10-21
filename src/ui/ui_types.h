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
    float_t x{ 0 };
    float_t y{ 0 };
    float_t w{ 100 };
    float_t h{ 100 };

    float_t rot{ 0 };

    struct Anchor
    {
        float_t x{ 0.5 };
        float_t y{ 0.5 };

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Anchor, x, y);
    } anchor;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Rect_transform, x, y, w, h, rot, anchor);
};

struct UI_file_render_image_data
{
    bool enabled{ false };
    std::string image_name;
    vec3s color_tint{ 1, 1, 1 };
    bool is_nine_slice{ false };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UI_file_render_image_data,
                                                enabled,
                                                image_name,
                                                color_tint,
                                                is_nine_slice);
};

struct UI_file_render_text_data
{
    bool enabled{ false };
    std::string text;
    std::string halign{ "center" };
    std::string valign{ "center" };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UI_file_render_text_data,
                                                enabled,
                                                text,
                                                halign,
                                                valign);
};

struct UI_file_element_data
{
    std::string name;
    std::string parent;
    Rect_transform transform;
    float_t opacity;
    UI_file_render_image_data render_image;
    UI_file_render_text_data render_text;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UI_file_element_data,
                                                name,
                                                parent,
                                                transform,
                                                opacity,
                                                render_image,
                                                render_text);
};

struct UI_file_data
{
    std::vector<UI_file_element_data> elements;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UI_file_data, elements);
};

constexpr uint32_t k_texture_idx_no_texture{ (uint32_t)-1 };

struct UI_element
{
    std::string name;
    UI_element* parent{ nullptr };
    Rect_transform transform;  // local transform.
    float_t opacity{ 1 };

    uint32_t texture_idx{ k_texture_idx_no_texture };
    vec3 color_tint{ 1, 1, 1 };
    bool is_nine_slice{ false };

    std::string render_text;

    enum Align : int32_t
    {
        LEFT = 0,
        CENTER,
        RIGHT,

        TOP = LEFT,
        BOTTOM = RIGHT,
    };
    Align halign;
    Align valign;
};

} // namespace UI
} // namespace TXP
