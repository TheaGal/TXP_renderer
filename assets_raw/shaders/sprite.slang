struct Sprite_data_set_element
{
    float4x4 model_mat;
    float2 anchor;
    float opacity;
    uint32_t texture_idx;
    uint32_t is_nine_slice;
    uint32_t pad0;
    uint32_t pad1;
    uint32_t pad2;
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
    float sprite_opacity;
    uint32_t sprite_texture_idx;
};


[shader("vertex")]
VS_output vertex_main(uniform Sprite_data_set* sprite_data_set,
                      uniform float camera_view_aspect_ratio,
                      uniform float render_canvas_height,
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
                         float4(k_unit_square_vertices[vertex_index] - sprite_data_elem.anchor,
                                0.0,
                                1.0));
        output.uv = k_unit_square_vertices[vertex_index];
    }

    float render_canvas_multiplier = (1.0f / render_canvas_height) * 2.0f;
    output.pos.x *= render_canvas_multiplier * (1.0f / camera_view_aspect_ratio);
    output.pos.y *= render_canvas_multiplier;

    output.sprite_opacity = sprite_data_elem.opacity;
    output.sprite_texture_idx = sprite_data_elem.texture_idx;

    return output;
}


[shader("fragment")]
float4 fragment_main(VS_output input)
{
    // Unlit sample from texture.
    float4 color = textures[NonUniformResourceIndex(input.sprite_texture_idx)]
                       .Sample(input.uv).rgba;
    return float4(color.rgb, color.a * input.sprite_opacity);
}
