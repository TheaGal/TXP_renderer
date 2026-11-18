#language slang 2026

struct VS_input
{
    float3 pos;
    float3 normal;
    float2 uv;
    float4 tangent;
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

struct VS_output
{
    float4 pos : SV_POSITION;
    float3 frag_pos;
};


[shader("vertex")]
VS_output vertex_main(VS_input input,
                      uniform float4x4 light_projection,
                      uniform float4x4 light_view,
                      uniform Per_instance_data_collection* per_instance_data_collection,
                      uniform Model_transform_set* model_transform_set,
                      uint instance_index : SV_VulkanInstanceID)
{
    const Per_instance_data per_inst_data =
        per_instance_data_collection->per_instance_datas[instance_index];
    const float4x4 model_mat =
        model_transform_set->transforms[per_inst_data.model_transform_set_idx];

    VS_output output;

    const float4 world_pos = mul(model_mat, float4(input.pos.xyz, 1.0));
    const float4 frag_pos = mul(light_view, world_pos);
    output.pos = mul(light_projection, frag_pos);

    output.frag_pos = frag_pos.xyz;

    return output;
}


[shader("fragment")]
float fragment_main(VS_output input)
{
    return input.frag_pos.z;
}
