#language slang 2026

struct VS_input
{
    float3 pos;
    float3 normal;
    float2 uv;
    float4 tangent;
};

struct Environment_data
{
    float4x4 projection;
    float4x4 view;
    float4 camera_position;
};

struct Directional_light
{
    float4 direction_xyz_intensity_w;
    float4 color;
};

struct Point_light
{
    float4 position_xyz_intensity_w;
    float4 color;
};

struct Lighting_data
{
    Directional_light directional_light;
    Point_light point_lights[128];
    uint32_t num_point_lights;
};

struct Per_instance_data
{
    uint32_t model_transform_set_idx;
    uint32_t material_param_set_idx;
};

struct Per_instance_data_collection
{
    Per_instance_data per_instance_datas[65535];
};

struct Model_transform_set
{
    float4x4 transforms[65535];
};

Sampler2D textures[];  // @NOCHECKIN: @CHECK: does this work??  @REPLY: it looks like it doesn't quite work? I'll need to look into it more huh.  -Thea 2026/09/27
// StructuredBuffer<DescriptorHandle<Sampler2D>> textures;  // try this method??
static const uint32_t TEX_IDX_NONE = (uint32_t)-1;

struct Material_param_set
{
    float4 base_color_factor;                  // base color tint/multiplier
    float metallic_factor;                     // metallic property multiplier
    float roughness_factor;                    // surface roughness multiplier
    uint32_t base_color_texture_idx;           // texture binding index for base color (-1 = none)
    uint32_t physical_descriptor_texture_idx;  // texture binding for metallic/roughness
    uint32_t normal_texture_idx;               // texture binding for normal maps
    uint32_t occlusion_texture_idx;            // texture binding for ambient occlusion
    uint32_t emissive_texture_idx;             // texture binding for emissive maps
    uint32_t pad0;
    float4 uv_offset_tiling;                   // x y are offset and z w are tiling
};

struct Material_param_set_collection
{
    Material_param_set material_param_sets[];
};

struct VS_output
{
    float4 pos : SV_POSITION;
    float3 world_pos;
    float3 normal;
    float2 uv;
    float4 tangent;
    float3 camera_pos;
    Lighting_data* light_data;
    uint32_t lighting_mode;
    Material_param_set mat_param_set;
};


[shader("vertex")]
VS_output vertex_main(VS_input input,
                      uniform Environment_data* env_data,
                      uniform Lighting_data* light_data,
                      uniform Per_instance_data_collection* per_instance_data_collection,
                      uniform Model_transform_set* model_transform_set,
                      uniform Material_param_set_collection* material_param_set_collection,
                      uniform uint32_t lighting_mode,
                      uint instance_index : SV_VulkanInstanceID)
{
    const Per_instance_data per_inst_data =
        per_instance_data_collection->per_instance_datas[instance_index];
    const float4x4 model_mat =
        model_transform_set->transforms[per_inst_data.model_transform_set_idx];
    const Material_param_set mat_param_set =
        material_param_set_collection->material_param_sets[per_inst_data.material_param_set_idx];

    VS_output output;

    const float4 world_pos = mul(model_mat, float4(input.pos.xyz, 1.0));
    const float4 frag_pos = mul(env_data->view, world_pos);
    output.pos = mul(env_data->projection, frag_pos);

    output.world_pos = world_pos.xyz / world_pos.w;
    output.normal = normalize(mul((float3x3)model_mat, input.normal));
    output.uv = (input.uv + mat_param_set.uv_offset_tiling.xy) * mat_param_set.uv_offset_tiling.zw;
    output.tangent = input.tangent;

    output.camera_pos = env_data->camera_position.xyz;

    output.light_data = light_data;
    output.lighting_mode = lighting_mode;

    output.mat_param_set = mat_param_set;

    return output;
}


// @REF: https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Lighting_Materials/04_lighting_implementation.html
static const float PI = 3.14159265359;


/// Normal Distribution Function (D) - GGX/Trowbridge-Reitz Distribution.
/// Describes the statistical distribution of microfacet orientations.
float distribution_ggx(float NdotH, float roughness)
{
    float a = roughness * roughness;  // remapping for more perceptual linearity
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float nom = a2;  // numerator: concentration factor
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;  // normalization factor

    return nom / denom;  // normalized distribution
}

/// Geometry Function (G) - Smith's method with Schlick-GGX approximation.
/// Models self-shadowing and masking between microfacets.
float geometry_smith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;  // direct lighting remapping

    // Geometry obstruction from view direction (masking)
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    // Geometry obstruction from light direction (shadowing)
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);

    return ggx1 * ggx2;  // combined masking-shadowing
}

/// Fresnel Reflectance (F) - Schlick's approximation.
/// Models how reflectance changes with viewing angle.
float3 fresnel_schlick(float cos_theta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}


/// Samples a texture at UV if the texture index is valid.
float4 sample_texture_idx(uint32_t texture_idx, float2 uv, float4 default_value)
{
    if (texture_idx == TEX_IDX_NONE)
        return default_value;
    
    return textures[NonUniformResourceIndex(texture_idx)].Sample(uv);
    // return textures[texture_idx].Sample(uv);  // try this method??
}

/// Samples a texture at UV if the texture index is valid.
float4 sample_texture_idx_tinted(uint32_t texture_idx, float2 uv, float4 tint)
{
    return (sample_texture_idx(texture_idx, uv, float4(1)) * tint);
}


/// Calculates light dir of directional light.
float3 calc_directional_light_dir(Directional_light light)
{
    return normalize(light.direction_xyz_intensity_w.xyz);
}

/// Calculates radiance of directional light.
float3 calc_directional_light_radiance(Directional_light light)
{
    return (light.color.rgb * light.direction_xyz_intensity_w.w);
}

/// Calculates light dir of point light.
float3 calc_point_light_dir(Point_light light, float3 world_pos)
{
    return normalize(light.position_xyz_intensity_w.xyz - world_pos);
}

/// Calcs radiance of point light.
float3 calc_point_light_radiance(Point_light light, float3 world_pos)
{
    const float3 delta_pos = (light.position_xyz_intensity_w.xyz - world_pos);

    float distance2 = dot(delta_pos, delta_pos);
    float attenuation = (light.position_xyz_intensity_w.w / distance2) - 0.1;  // @HARDCODE

    return max(float3(0), light.color.rgb * attenuation);
}


/// Lighting for PBR.
float3 pbr_lighting(VS_output input, float4 base_color)
{
    // --- MATERIAL PROPERTY SAMPLING --------------------------------------------------------------
    // Sample metallic-roughness texture (metallic=B channel, roughness=G channel)
    // glTF standard: metallic stored in blue, roughness in green
    const float2 metallic_roughness =
        sample_texture_idx(input.mat_param_set.physical_descriptor_texture_idx,
                           input.uv,
                           float4(1)).bg;
    const float metallic = metallic_roughness.x * input.mat_param_set.metallic_factor;
    const float roughness = metallic_roughness.y * input.mat_param_set.roughness_factor;

    // Sample ambient occlusion (typically stored in red channel)
    const float ao =
        sample_texture_idx(input.mat_param_set.occlusion_texture_idx, input.uv, float(1)).r;

    // Sample emissive texture for self-illuminating materials
    const float3 emissive = sample_texture_idx(input.mat_param_set.emissive_texture_idx,
                                               input.uv,
                                               float4(0)).rgb;

    // --- NORMAL CALCULATION ----------------------------------------------------------------------
    // Start with interpolated surface normal
    float3 N = normalize(input.normal);

    // Apply normal mapping if texture is available
    if (input.mat_param_set.normal_texture_idx != TEX_IDX_NONE)
    {
        // Sample normal map and convert from [0,1] to [-1,1] range
        const float3 tangent_normal =
            sample_texture_idx(input.mat_param_set.normal_texture_idx, input.uv, float4(0)).xyz *
                2.0 -
            1.0;

        const float3 T = normalize(input.tangent.xyz);  // tangent
        const float3 B = normalize(cross(N, T)) * input.tangent.w;  // bitangent (w = handedness)
        const float3x3 TBN = float3x3(T, B, N);  // Tangent-Bitangent-Normal matrix

        // Transform normal from tangent space to world space
        N = normalize(mul(tangent_normal, TBN));
    }

    // --- LIGHTING SETUP --------------------------------------------------------------------------
    // Calculate view direction (fragment to camera)
    const float3 V = normalize(input.camera_pos - input.world_pos);

    // Calculate reflection vector for environment mapping
    const float3 R = reflect(-V, N);

    // --- PBR MATERIAL SETUP ----------------------------------------------------------------------
    // Calculate F0 (reflectance at normal incidence)
    // Non-metals: low reflectance (~0.04), Metals: colored reflectance from base color
    float3 F0 = float3(0.04, 0.04, 0.04);  // dielectric default
    F0 = lerp(F0, base_color.rgb, metallic);  // lerp to metallic behavior

    // Initialize outgoing radiance accumulator
    float3 Lo = float3(0.0, 0.0, 0.0);

    // --- DIRECT LIGHTING LOOP --------------------------------------------------------------------
    // Calculate contribution from each light source
    [loop]
    for (int i = 0; i < 1 + input.light_data->num_point_lights; i++)
    {
        // Calculate light direction and attenuation
        const float3 L =
            (i == 0 ? calc_directional_light_dir(input.light_data->directional_light)
                    : calc_point_light_dir(input.light_data->point_lights[i - 1], input.world_pos));
        const float3 radiance =
            (i == 0 ? calc_directional_light_radiance(input.light_data->directional_light)
                    : calc_point_light_radiance(input.light_data->point_lights[i - 1], input.world_pos));

        // Calculate half vector (between view and light directions)
        const float3 H = normalize(V + L);

        // --- BRDF EVALUATION ---------------------------------------------------------------------
        // Calculate all necessary dot products for BRDF terms
        const float NdotL = max(dot(N, L), 0.0);  // lambertian falloff
        const float NdotV = max(dot(N, V), 0.0);  // view angle
        const float NdotH = max(dot(N, H), 0.0);  // half vector for specular
        const float HdotV = max(dot(H, V), 0.0);  // for fresnel calculation

        // Evaluate Cook-Torrance BRDF components
        const float D = distribution_ggx(NdotH, roughness);       // normal distribution
        const float G = geometry_smith(NdotV, NdotL, roughness);  // geometry function
        const float3 F = fresnel_schlick(HdotV, F0);              // fresnel reflectance

        // Calculate specular BRDF
        const float3 numerator = D * G * F;
        const float denominator = 4.0 * NdotV * NdotL + 0.0001;  // prevent division by zero
        const float3 specular = numerator / denominator;

        // --- ENERGY CONSERVATION -----------------------------------------------------------------
        // Fresnel term represents specular reflection ratio
        const float3 kS = F;                     // specular contribution
        float3 kD = float3(1.0, 1.0, 1.0) - kS;  // diffuse contribution (energy conservation)
        kD *= 1.0 - metallic;                    // metals have no diffuse reflection

        // --- RADIANCE ACCUMULATION ---------------------------------------------------------------
        // Combine diffuse (Lambertian) and specular (Cook-Torrance) terms
        // Multiply by incident radiance and cosine foreshortening
        Lo += (kD * base_color.rgb / PI + specular) * radiance * NdotL;
    }

    // --- AMBIENT AND EMISSIVE --------------------------------------------------------------------
    // Add simple ambient lighting (should be replaced with IBL in production)
    const float3 ambient = float3(0.03, 0.03, 0.03) * base_color.rgb * ao;

    // Combine all lighting contributions
    return (ambient + Lo + emissive);
}


/// Lighting for simple lit.
float3 simple_lit_lighting(VS_output input, float4 base_color)
{
    float3 N = normalize(input.normal);
    float3 L = calc_directional_light_dir(input.light_data.directional_light);
    float3 diffuse = max(dot(N, L) * 0.5 + 0.5, 0);

    constexpr float base_lightness = 0.125;
    float3 kinda_gooch_diffuse = (diffuse * (1.0 - base_lightness) + base_lightness);

    return (kinda_gooch_diffuse * base_color.rgb);
}


[shader("fragment")]
float4 fragment_main(VS_output input)
{
    // Sample base color texture and apply material color factor
    const float4 base_color = sample_texture_idx_tinted(input.mat_param_set.base_color_texture_idx,
                                                        input.uv,
                                                        input.mat_param_set.base_color_factor);

    float3 color = float3(1, 0, 1);  // error debug color

    // PBR lighting mode.
    if (input.lighting_mode == 0)
    {
        color = pbr_lighting(input, base_color);
    }
    // Simple lit lighting mode.
    else if (input.lighting_mode == 1)
    {
        color = simple_lit_lighting(input, base_color);
    }

    // Output final color.
    return float4(color, base_color.a);
}
