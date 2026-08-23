struct VS_input
{
    float3 pos;
    float4 color;
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

struct VS_output
{
    float4 pos : SV_POSITION;
    float4 color;
};


[shader("vertex")]
VS_output vertex_main(VS_input input,
                      uniform Environment_data* env_data)
{
    VS_output output;
    output.pos = mul(env_data->projection,
                     mul(env_data->view,
                         float4(input.pos.xyz, 1.0)));
    output.color = input.color;

    return output;
}


[shader("fragment")]
float4 fragment_main(VS_output input)
{
    return float4(input.color.rgb, 1.0);
}
