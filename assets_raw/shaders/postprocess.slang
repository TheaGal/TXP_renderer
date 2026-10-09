#language slang 2026

cbuffer Uniforms
{
    Texture2D<half4> color_image;
    Texture2D<float4> ui_image;
    WTexture2D<float4> dest_image;
}


#define MANUAL_SRGB 1

half3 uncharted_2_tonemap(half3 color)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return half3(((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F))
                 - E / F);
}

half4 tonemap(half4 color, float exposure, float gamma)
{
    half3 outcol = uncharted_2_tonemap(half3(color.rgb * exposure));
    outcol = half3(outcol * (1.0 / uncharted_2_tonemap(half3(11.2))));
    return half4(pow(outcol, half3(1.0f / gamma)), color.a);
}

half4 srgb_to_linear(half4 srgb_in)
{
#ifdef MANUAL_SRGB
  #ifdef SRGB_FAST_APPROXIMATION
    float3 lin_out = pow(srgb_in.xyz, float3(2.2));
  #else // SRGB_FAST_APPROXIMATION
    float3 bLess = step(float3(0.04045), srgb_in.xyz);
    float3 lin_out = lerp(srgb_in.xyz / float3(12.92),
                          pow((srgb_in.xyz + float3(0.055)) / float3(1.055), float3(2.4)),
                          bLess);
  #endif // SRGB_FAST_APPROXIMATION
    return half4(half3(lin_out), srgb_in.w);
#else // MANUAL_SRGB
    return srgb_in;
#endif // MANUAL_SRGB
}

half3 apply_vignette(const half3 in_color, const half2 screen_uv)
{
    static const half vignette_strength = 0.75h;
    half dist_from_center = min(1.0h, length(screen_uv) - 0.5h);
    return lerp(in_color, half3(0), dist_from_center * vignette_strength);
}


[shader("compute")]
[numthreads(16, 16, 1)]
void compute_main(uniform uint32_t use_ui_image,
                  uniform float exposure,
                  uniform float gamma,
                  uint3 thread_idx : SV_DispatchThreadID,
                  uint3 local_thread_idx : SV_GroupThreadID)
{
    uint color_image_w, color_image_h;
    color_image.GetDimensions(color_image_w, color_image_h);

    if (thread_idx.x < color_image_w && thread_idx.y < color_image_h)
    {
        // Tonemap HDR image.
        half4 raw_color = color_image.Load(thread_idx);
        half3 color = srgb_to_linear(tonemap(raw_color, exposure, gamma)).rgb;
        color = clamp(color, half3(0), half3(1));

        // Vignette.
        if (bool(use_ui_image))
        {
            half2 screen_uv = half2(half(thread_idx.x) / half(color_image_w),
                                    half(thread_idx.y) / half(color_image_h));
            screen_uv = screen_uv * 2 - 1;

            color = apply_vignette(color, screen_uv);
        }

        // Incorporate UI.
        half4 ui_color = half4(0);
        if (bool(use_ui_image))
            ui_color = half4(ui_image.Load(thread_idx));
        color = lerp(color, ui_color.rgb, ui_color.a);

        // Spit it out!
        dest_image.Store(uint2(thread_idx.x, thread_idx.y), float4(color, 1.0));
    }
}
