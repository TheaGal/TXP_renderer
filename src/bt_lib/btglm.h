#pragma once

// #include "../btzc_game_engine.h"  // @NOTE: @THEA: @NOCHECKIN: this is where the double precision thing comes from.
#include "btjson.h"
#include "cglm/cglm.h"
#include "cglm/quat.h"
#include "cglm/types-struct.h"

#include <cmath>


/// Whether to use double or single precision for real type.
//  @NOTE: There's an issue where `using rvec3 = real_t[3];` causes errors. This is due to the type
//         alias being something not quite the same type. It shows up as `AKA double[3]` in clang.
//         Due to this, it cannot be used in initializer lists. So using a typedef actually sets the
//         type to the other type without any soft aliases, thus causing no issue when using in an
//         initializer list.
//           -Thea 2025/10/21
#ifdef BTZC_REAL_TYPE
#error This macro `BTZC_REAL_TYPE` already defined!!!
#endif  // BTZC_REAL_TYPE

#if BTZC_GAME_ENGINE_SETTING_REAL_TYPE_USES_DBL_PRECISION
#define BTZC_REAL_TYPE double_t
#else
#define BTZC_REAL_TYPE float_t
#endif  // BTZC_GAME_ENGINE_SETTING_REAL_TYPE_USES_DBL_PRECISION

typedef BTZC_REAL_TYPE real_t;
typedef BTZC_REAL_TYPE rvec3[3];
#undef BTZC_REAL_TYPE


/// Struct RVec3 type.
union rvec3s
{
    rvec3 raw;
    struct
    {
        real_t x;
        real_t y;
        real_t z;
    };
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(rvec3s, x, y, z);


/// RVec3 funcs.
inline void btglm_rvec3_copy(rvec3 const v, rvec3 dest)
{
    dest[0] = v[0];
    dest[1] = v[1];
    dest[2] = v[2];
}

inline void btglm_rvec3_negate_to(rvec3 const v, rvec3 dest)
{
    dest[0] = -v[0];
    dest[1] = -v[1];
    dest[2] = -v[2];
}

inline void btglm_rvec3_add(rvec3 const a, rvec3 const b, rvec3 dest)
{
    dest[0] = a[0] + b[0];
    dest[1] = a[1] + b[1];
    dest[2] = a[2] + b[2];
}

inline void btglm_rvec3_sub(rvec3 const a, rvec3 const b, rvec3 dest)
{
    dest[0] = a[0] - b[0];
    dest[1] = a[1] - b[1];
    dest[2] = a[2] - b[2];
}

inline void btglm_rvec3_scale_v3(rvec3 const v, vec3 const sca, rvec3 dest)
{
    dest[0] = v[0] * sca[0];
    dest[1] = v[1] * sca[1];
    dest[2] = v[2] * sca[2];
}

inline void btglm_quat_mul_rvec3(versor const q, rvec3 const v, rvec3 dest)
{
    mat3 m;
    if constexpr (false)
    {
        versor norm_q;
        glm_quat_normalize_to(const_cast<float_t*>(q), norm_q);
        glm_quat_mat3(norm_q, m);
    }
    else
    {
        glm_quat_mat3(const_cast<float_t*>(q), m);
    }

    // glm_mat3_mulv(mat3 m, vec3 v, vec3 dest) {
    rvec3 res;
    res[0] = m[0][0] * v[0] + m[1][0] * v[1] + m[2][0] * v[2];
    res[1] = m[0][1] * v[0] + m[1][1] * v[1] + m[2][1] * v[2];
    res[2] = m[0][2] * v[0] + m[1][2] * v[1] + m[2][2] * v[2];
    // glm_vec3_copy(res, dest);
    btglm_rvec3_copy(res, dest);
    // }
}


/// Types from cglm.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec2s, x, y);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec3s, x, y, z);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec4s, x, y, z, w);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ivec2s, x, y);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ivec3s, x, y, z);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ivec4s, x, y, z, w);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(versors, x, y, z, w);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(mat4s, m00, m01, m02, m03,
                                          m10, m11, m12, m13,
                                          m20, m21, m22, m23,
                                          m30, m31, m32, m33);
