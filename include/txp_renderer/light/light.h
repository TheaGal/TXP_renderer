#pragma once

#include "btglm.h"
#include "btjson.h"

#include <cmath>


namespace TXP
{
namespace component
{

/// Component for directional light.
struct Directional_light
{
    vec3s direction{ 0.742781, 0.557086, 0.371391 };
    vec3s color{ 1, 1, 1 };
    float_t intensity{ 1 };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        Directional_light,
        direction,
        color,
        intensity
    );
};

/// Component for point light.
struct Point_light
{
    vec3s origin{ 0, 0, 0 };
    vec3s color{ 1, 1, 1 };
    float_t intensity{ 1 };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        Point_light,
        origin,
        color,
        intensity
    );
};

}  // namespace component
}  // namespace TXP
