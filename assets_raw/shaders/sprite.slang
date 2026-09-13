// // @THEA: Delete this struct??
// struct VS_input
// {
//     float3 pos;
//     float3 normal;
//     float2 uv;
// };
// /////////////////////////





struct Sprite_data_set_element
{
    float4x4 model_mat;
    uint32_t texture_idx;
    uint32_t is_nine_slice;
    uint32_t pad0;
    uint32_t pad1;
};

struct Sprite_data_set
{
    Sprite_data_set_element per_instance_sprites[8192];
};

Sampler2D textures[];

struct VS_output
{
    float4 pos : SV_POSITION;
    float2 uv;
    uint32_t sprite_texture_idx;
};


[shader("vertex")]
VS_output vertex_main(//VS_input input,
                      uniform Sprite_data_set* sprite_data_set,
                      uniform float camera_view_aspect_ratio,
                      uint vertex_index : SV_VertexID,
                      uint instance_index : SV_VulkanInstanceID)
{
    Sprite_data_set_element sprite_data_elem =
        sprite_data_set->per_instance_sprites[instance_index];

    // @NOTE: use these for UVs as well.
    constexpr float2 k_unit_square_vertices[] = {
        float2(0, 0),
        float2(0, 1),
        float2(1, 1),

        float2(0, 0),
        float2(1, 1),
        float2(1, 0),
    };

    VS_output output;
    if (sprite_data_elem.is_nine_slice == 1)
    {
        // @TODO
    }
    else
    {
        output.pos = mul(sprite_data_elem.model_mat,
                         float4(k_unit_square_vertices[vertex_index], 0.0, 1.0));
        output.uv = k_unit_square_vertices[vertex_index];
    }
    output.pos.x *= (1.0f / camera_view_aspect_ratio);

    output.sprite_texture_idx = sprite_data_elem.texture_idx;

    return output;
}


[shader("fragment")]
float4 fragment_main(VS_output input)
{
    // Unlit sample from texture.
    float4 color = textures[NonUniformResourceIndex(input.sprite_texture_idx)]
                       .Sample(input.uv).rgba;
    return color;
}
