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

struct Material_param_set
{
    float4 color;
};

struct Material_param_set_collection
{
    Material_param_set material_param_sets[];
};

struct VS_output
{
    float4 pos : SV_POSITION;
    float3 normal;
    // float3 bary_coords;  @THEA: doesn't quite work.
    float3 light_vec;
    Material_param_set mat_param_set;
};


[shader("vertex")]
VS_output vertex_main(VS_input input,
                      uniform Environment_data* env_data,
                      uniform Per_instance_data_collection* per_instance_data_collection,
                      uniform Model_transform_set* model_transform_set,
                      uniform Material_param_set_collection* material_param_set_collection,
                    //   uint vert_index : SV_VertexID,
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
    output.normal = mul((float3x3)model_mat, input.normal);

    // output.bary_coords = float3(fmod(vert_index, 3) == 0 ? 1 : 0,
    //                             fmod(vert_index, 3) == 1 ? 1 : 0,
    //                             fmod(vert_index, 3) == 2 ? 1 : 0);

    output.light_vec = env_data->directional_light.direction_xyz_intensity_w.xyz;

    output.mat_param_set =
        material_param_set_collection->material_param_sets[per_inst_data.material_param_set_idx];

    return output;
}


float edge_factor(float3 bary_coords)
{
    const float k_line_width = 1;

    float3 d = fwidth(bary_coords);
    float3 f = step(d * k_line_width, bary_coords);
    return min(min(f.x, f.y), f.z);
}

[shader("fragment")]
float4 fragment_main(VS_output input)
{
    // Rando diffuse.
    float3 N = normalize(input.normal);
    float3 L = normalize(input.light_vec);
    float3 diffuse = max(dot(N, L) * 0.5 + 0.5, 0.1);

    // return float4(min(float3(edge_factor(input.bary_coords)), diffuse * input.mat_param_set.color.rgb), input.mat_param_set.color.a);
    return float4(diffuse * input.mat_param_set.color.rgb, input.mat_param_set.color.a);
}
