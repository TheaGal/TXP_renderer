#language slang 2026

cbuffer Uniforms
{
    Texture2D<half4> color_image;
    Texture2D<uint8_t4> ui_image;
    WTexture2D<half4> dest_image;
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


[shader("compute")]
[numthreads(16, 16, 1)]
void compute_main(uniform float exposure,
                  uniform float gamma,
                  uint3 thread_idx : SV_DispatchThreadID,
                  uint3 local_thread_idx : SV_GroupThreadID)
{
    uint color_image_w, color_image_h;
    color_image.GetDimensions(color_image_w, color_image_h);

    if (thread_idx.x < color_image_w && thread_idx.y < color_image_h)
    {
        half4 raw_color = color_image.Load(thread_idx);
        half3 color = srgb_to_linear(tonemap(raw_color, exposure, gamma)).rgb;

        uint8_t4 ui_color = ui_image.Load(thread_idx);

        dest_image.Store(uint2(thread_idx.x, thread_idx.y),
                         half4(lerp(color, half3(ui_color.rgb), half(ui_color.a)), half(1.0)));
    }
}
