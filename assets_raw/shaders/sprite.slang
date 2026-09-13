struct VS_input
{
    float3 pos;
    float3 normal;
    float2 uv;
};

struct Directional_light
{
    float4 direction_xyz_intensity_w;
    float4 color;
};

struct Environment_data
{
    float4x4 projection;
    float4x4 view;
    Directional_light directional_light;
    uint32_t lighting_mode;
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

Sampler2D textures[];

struct Material_param_set
{
    uint32_t texture0_idx;
};

struct Material_param_set_collection
{
    Material_param_set material_param_sets[];
};

struct VS_output
{
    float4 pos : SV_POSITION;
    float3 normal;
    float2 uv;
    float3 view_vec;
    float3 light_vec;
    uint32_t lighting_mode;
    Material_param_set mat_param_set;
};


[shader("vertex")]
VS_output vertex_main(VS_input input,
                      uniform Environment_data* env_data,
                      uniform Per_instance_data_collection* per_instance_data_collection,
                      uniform Model_transform_set* model_transform_set,
                      uniform Material_param_set_collection* material_param_set_collection,
                      uint instance_index : SV_VulkanInstanceID)
{
    Per_instance_data per_inst_data =
        per_instance_data_collection->per_instance_datas[instance_index];
    float4x4 model_mat = model_transform_set->transforms[per_inst_data.model_transform_set_idx];

    VS_output output;
    output.pos = mul(env_data->projection,
                     mul(env_data->view,
                         mul(model_mat,
                             float4(input.pos.xyz, 1.0))));
    output.normal = mul((float3x3)mul(env_data->view, model_mat), input.normal);
    output.uv = input.uv;
    output.lighting_mode = env_data->lighting_mode;

    output.mat_param_set =
        material_param_set_collection->material_param_sets[per_inst_data.material_param_set_idx];

    // Calculate view vectors required for lighting.
    float4 frag_pos = mul(mul(env_data->view, model_mat), float4(input.pos.xyz, 1.0));
    output.view_vec = -frag_pos.xyz;
    output.light_vec = env_data->directional_light.direction_xyz_intensity_w.xyz - frag_pos.xyz;
    return output;
}


[shader("fragment")]
float4 fragment_main(VS_output input)
{
    constexpr float4 k_error_color = float4(1, 0, 1, 1);

    if (input.lighting_mode == 0)
    {   // PBR.
        // @TODO: implement.
        return k_error_color;
    }
    else if (input.lighting_mode == 1)
    {   // Thea-Phong lighting.
        float3 N = normalize(input.normal);
        float3 L = normalize(input.light_vec);
        float3 V = normalize(input.view_vec);
        float3 R = reflect(-L, N);
        float3 diffuse = max(dot(N, L) * 0.5 + 0.5, 0.0025);
        float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;

        // Sample from texture.
        float3 color = textures[NonUniformResourceIndex(input.mat_param_set.texture0_idx)]
                           .Sample(input.uv).rgb;
        return float4(diffuse * color.rgb + specular, 1.0);
    }
    else if (input.lighting_mode == 2)
    {   // Unlit sample from texture.
        float3 color = textures[NonUniformResourceIndex(input.mat_param_set.texture0_idx)]
                           .Sample(input.uv).rgb;
        return float4(color.rgb, 1.0);
    }

    // Error: invalid `lighting_mode` value.
    return k_error_color;
}
