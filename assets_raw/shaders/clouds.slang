#language slang 2026

cbuffer Uniforms
{
    Texture3D<float> voxel_grid;
    Texture2D<float> depth_image;  // @TODO: make this half res in the future!
    WTexture2D<float4> dest_image;
}


[shader("compute")]
[numthreads(16, 16, 1)]
void compute_main(uniform float4x4 inv_cam_projview,
                  uniform float3 voxel_grid_origin,
                  uniform float voxel_size,
                  uint3 thread_idx : SV_DispatchThreadID,
                  uint3 local_thread_idx : SV_GroupThreadID)
{
    uint3 voxel_grid_size;
    voxel_grid.GetDimensions(voxel_grid_size.x, voxel_grid_size.y, voxel_grid_size.z);

    uint dest_image_w, dest_image_h;
    dest_image.GetDimensions(dest_image_w, dest_image_h);

    if (thread_idx.x < dest_image_w && thread_idx.y < dest_image_h)
    {
        // Calc starting raymarch position.
        float4 raymarch_pos;
        {
            raymarch_pos = float4(((float)thread_idx.x / (float)dest_image_w) * 2 - 1,
                                  ((float)thread_idx.y / (float)dest_image_h) * 2 - 1,
                                  1,  // 1 for reverse depth
                                  1);
            raymarch_pos = mul(inv_cam_projview, raymarch_pos);
            raymarch_pos /= raymarch_pos.w;
        }

        // Get nearest z value in depth image.
        // @TODO: if this is already precalc then this wouldn't be needed!
        float depth = -1;
        depth = max(depth, depth_image.Load(thread_idx * 2, int2(0, 0)));
        depth = max(depth, depth_image.Load(thread_idx * 2, int2(1, 0)));
        depth = max(depth, depth_image.Load(thread_idx * 2, int2(0, 1)));
        depth = max(depth, depth_image.Load(thread_idx * 2, int2(1, 1)));

        // Ensure that depth used is marginally closer than the camera's z-near.
        if (depth > 1.0 - 1e-6)
            return;

        // @DEBUG: just write a debug value.
        dest_image.Store(thread_idx.xy, float4(raymarch_pos.x, raymarch_pos.y, depth, 1));
    }
}
