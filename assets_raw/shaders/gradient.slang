#language slang 2026

cbuffer Uniforms
{
    WTexture2D<half4> image;
}


[shader("compute")]
[numthreads(16, 16, 1)]
void compute_main(uint3 thread_idx : SV_DispatchThreadID)
{
    uint x = thread_idx.x;
    uint y = thread_idx.y;

    uint w, h;
    image.GetDimensions(w, h);

    if (x < w && y < h)
    {
        half4 color = half4(0, 0, 0, 1);
        if (x != 0 && y != 0)
        {
            color.x = half(x) / half(w);
            color.y = half(y) / half(h);
        }

        image.Store(uint2(x, y), color);
    }
}
