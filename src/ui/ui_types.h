#pragma once

#include "btjson.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>


namespace TXP
{
namespace UI
{

struct Rect_transform
{
    float_t x;
    float_t y;
    float_t w;
    float_t h;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Rect_transform, x, y, w, h);
};

struct UI_file_element_data
{
    std::string name;
    std::string parent;
    Rect_transform transform;
    float_t opacity;
    std::string image;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UI_file_element_data, name, parent, transform, opacity, image);
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
    Rect_transform transform;
    float_t opacity;
    uint32_t texture_idx;
};

} // namespace UI
} // namespace TXP
