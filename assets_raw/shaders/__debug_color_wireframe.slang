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
    float4 color;
};

struct Material_param_set_collection
{
    Material_param_set material_param_sets[];
};

struct VS_output
{
    float4 pos : SV_POSITION;
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
    output.mat_param_set =
        material_param_set_collection->material_param_sets[per_inst_data.material_param_set_idx];

    return output;
}


[shader("fragment")]
float4 fragment_main(VS_output input)
{
    return input.mat_param_set.color;
}
