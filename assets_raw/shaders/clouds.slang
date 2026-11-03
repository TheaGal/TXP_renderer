#language slang 2026

cbuffer Uniforms
{
    Texture3D<float> voxel_grid;
    Texture2D<float> depth_image;  // @TODO: make this half res in the future!
    WTexture2D<float4> dest_image;
}


float3 calc_start_raymarch_pos(float3 voxel_grid_origin,
                               float4x4 inv_cam_projview,
                               float2 screen_pos)
{
    return calc_end_raymarch_pos(voxel_grid_origin,
                                 inv_cam_projview,
                                 screen_pos,
                                 1.0);  // 1 for reverse depth
}

float3 calc_end_raymarch_pos(float3 voxel_grid_origin,
                             float4x4 inv_cam_projview,
                             float2 screen_pos,
                             float depth)
{
    float4 raymarch_pos;
    {
        raymarch_pos = float4(screen_pos, depth, 1);
        raymarch_pos = mul(inv_cam_projview, raymarch_pos);
        raymarch_pos /= raymarch_pos.w;
    }

    return (raymarch_pos.xyz - voxel_grid_origin);
}

float calc_depth_at_screen_pos(uint3 thread_idx)
{
    // Get nearest z value in depth image.
    // @TODO: if this is already precalc then this wouldn't be needed!
    const int3 sample_pos = int3(thread_idx.x * 2, thread_idx.y * 2, 0);

    float depth = -1;
    depth = max(depth, depth_image.Load(sample_pos, int2(0, 0)));
    depth = max(depth, depth_image.Load(sample_pos, int2(1, 0)));
    depth = max(depth, depth_image.Load(sample_pos, int2(0, 1)));
    depth = max(depth, depth_image.Load(sample_pos, int2(1, 1)));

    return depth;
}

float sqr_distance(float3 a, float3 b)
{
    float3 delta = (a - b);
    return dot(delta, delta);
}

float2 intersect_aabb(float3 ray_origin, float3 one_over_ray_dir, float3 aabb_min, float3 aabb_max)
{
    float2 t_minmax = float2(0, 1.#INF);

    [unroll]
    for (uint i = 0; i < 3; i++)
    {
        float t1 = (aabb_min[i] - ray_origin[i]) * one_over_ray_dir[i];
        float t2 = (aabb_max[i] - ray_origin[i]) * one_over_ray_dir[i];
        t_minmax.x = max(t_minmax.x, min(t1, t2));
        t_minmax.y = min(t_minmax.y, max(t1, t2));
    }

    return t_minmax;
}

struct Voxel_grid_info
{
    uint3 grid_dimensions;
    float voxel_size;
    float one_over_voxel_size;
};

uint3 calc_voxel_coord(Voxel_grid_info voxel_grid_info, float3 raymarch_pos)
{
    float3 grid_pos_f = (raymarch_pos * voxel_grid_info.one_over_voxel_size);

    int3 grid_pos_i = int3(grid_pos_f);
    max(grid_pos_i, int3(0));
    min(grid_pos_i, voxel_grid_info.grid_dimensions - int3(1));

    return uint3(grid_pos_i);
}

uint sample_voxel_at_coord(uint3 voxel_coord)
{
    const int4 sample_pos = int4(voxel_coord.x, voxel_coord.y, voxel_coord.z, 0);

    float raw_val = voxel_grid.Load(sample_pos);
    return uint(raw_val * 255);
}

float traverse(inout uint3 voxel_coord,
               float voxel_size,
               float3 raymarch_pos,
               float3 raymarch_dir,
               float3 one_over_raymarch_dir)
{
    // Find target planes.
    float3 target_planes = float3(voxel_coord * voxel_size);
    if (raymarch_dir.x > 0)
        target_planes.x += voxel_size;
    if (raymarch_dir.y > 0)
        target_planes.y += voxel_size;
    if (raymarch_dir.z > 0)
        target_planes.z += voxel_size;

    // Find intersect t for each plane.
    float3 t_max = float3(
        (target_planes.x - raymarch_pos.x) * one_over_raymarch_dir.x,
        (target_planes.y - raymarch_pos.y) * one_over_raymarch_dir.y,
        (target_planes.z - raymarch_pos.z) * one_over_raymarch_dir.z);
    
    float chosen_t_max;
    if (t_max.x < t_max.y)
    {
        if (t_max.x < t_max.z)
        {
            chosen_t_max = t_max.x;
            voxel_coord.x += sign(raymarch_dir.x);
        }
        else // t_max.z <= t_max.x
        {
            chosen_t_max = t_max.z;
            voxel_coord.z += sign(raymarch_dir.z);
        }
    }
    else // t_max.y <= t_max.x
    {
        if (t_max.y < t_max.z)
        {
            chosen_t_max = t_max.y;
            voxel_coord.y += sign(raymarch_dir.y);
        }
        else // t_max.z <= t_max.y
        {
            chosen_t_max = t_max.z;
            voxel_coord.z += sign(raymarch_dir.z);
        }
    }

    return chosen_t_max;
}


static const uint k_debug_render = 2;


[shader("compute")]
[numthreads(16, 16, 1)]
void compute_main(uniform float4x4 inv_cam_projview,
                  uniform float3 voxel_grid_origin,
                  uniform float voxel_size,
                  uint3 thread_idx : SV_DispatchThreadID,
                  uint3 local_thread_idx : SV_GroupThreadID)
{
    Voxel_grid_info voxel_grid_info;
    voxel_grid_info.voxel_size = voxel_size;
    voxel_grid_info.one_over_voxel_size = (1.0 / voxel_size);

    voxel_grid.GetDimensions(voxel_grid_info.grid_dimensions.x,
                             voxel_grid_info.grid_dimensions.y,
                             voxel_grid_info.grid_dimensions.z);

    uint dest_image_w, dest_image_h;
    dest_image.GetDimensions(dest_image_w, dest_image_h);

    if (thread_idx.x < dest_image_w && thread_idx.y < dest_image_h)
    {
        const float2 screen_pos = float2(((float)thread_idx.x / (float)dest_image_w) * 2 - 1,
                                         ((float)thread_idx.y / (float)dest_image_h) * 2 - 1);

        // Calc raymarching start props.
        float3 raymarch_pos;
        float3 raymarch_dir;
        float3 one_over_raymarch_dir;
        float raymarch_dist2;
        {
            raymarch_pos = calc_start_raymarch_pos(voxel_grid_origin, inv_cam_projview, screen_pos);

            const float depth = calc_depth_at_screen_pos(thread_idx);

            // // Ensure that depth used is marginally closer than the camera's z-near.
            // if (depth > 1.0 - 1e-6)
            //     return;

            float3 raymarch_end_pos = calc_end_raymarch_pos(voxel_grid_origin,
                                                            inv_cam_projview,
                                                            screen_pos,
                                                            depth);

            raymarch_dir = normalize(raymarch_end_pos - raymarch_pos);

            [unroll]
            for (uint i = 0; i < 3; i++)
            {
                one_over_raymarch_dir[i] =
                    (abs(raymarch_dir[i]) < 1e-9 ? 1.#INF : 1.0 / raymarch_dir[i]);
            }

            raymarch_dist2 = sqr_distance(raymarch_pos, raymarch_end_pos);
        }

        // Find intersection w AABB for voxel field.
        float2 aabb_t_minmax = intersect_aabb(raymarch_pos,
                                              one_over_raymarch_dir,
                                              float3(0),
                                              voxel_grid_info.voxel_size *
                                                  voxel_grid_info.grid_dimensions);
        if (aabb_t_minmax.x > aabb_t_minmax.y ||
            aabb_t_minmax.x * aabb_t_minmax.x > raymarch_dist2)
        {
            // No intersection w aabb or blocked by depth.
            dest_image.Store(thread_idx.xy, float4(0, 0, 0, 0));
            return;
        }

        raymarch_dist2 = min(raymarch_dist2, aabb_t_minmax.y * aabb_t_minmax.y);

        // Get first voxel position.
        raymarch_pos += (raymarch_dir * aabb_t_minmax.x);
        float traversed_dist = aabb_t_minmax.x;

        uint3 voxel_coord = calc_voxel_coord(voxel_grid_info, raymarch_pos);
        uint voxel_value = 0;
        float transmittance = 1;

        [loop]
        while (traversed_dist * traversed_dist <= raymarch_dist2 && (k_debug_render != 1 || voxel_value == 0))
        {
            // @NOTE: sample before traversal to prevent samples when traveling too far.
            voxel_value = sample_voxel_at_coord(voxel_coord);
            if (k_debug_render == 1 && voxel_value != 0)
                break;  // found a hit!

            // March forward~!
            float t = traverse(voxel_coord,
                               voxel_grid_info.voxel_size,
                               raymarch_pos,
                               raymarch_dir,
                               one_over_raymarch_dir);
            raymarch_pos += (raymarch_dir * t);
            traversed_dist += t;

            if (voxel_value != 0)
            {
                // Mix in density.
                const float k_voxel_density = 0.1;
                const float light_absorption_through_cloud = 0.2;

                const float d = (k_voxel_density * t * light_absorption_through_cloud);

                // lightEnergy += density * stepSize * transmittance * (1.0 - exp(-d * 2.0)) * inScatterTransmittance * phaseValue;       // Beer's-Powder approximation (https://www.guerrilla-games.com/media/News/Files/The-Real-time-Volumetric-Cloudscapes-of-Horizon-Zero-Dawn.pdf PAGE 64)
                transmittance *= exp(-d);

                if (transmittance < 1e-2)
                    break;
            }
        }

        if (k_debug_render == 0)
        {
            // Normal render.
            dest_image.Store(thread_idx.xy, float4(1, 0, 1, 1));  // @TODO: just put out error color.
        }
        else if (k_debug_render == 1)
        {
            // @DEBUG: just write a debug value of the collided voxel.
            dest_image.Store(thread_idx.xy,
                            float4((float)voxel_coord.x / (float)voxel_grid_info.grid_dimensions.x,
                                   (float)voxel_coord.y / (float)voxel_grid_info.grid_dimensions.y,
                                   (float)voxel_coord.z / (float)voxel_grid_info.grid_dimensions.z,
                                   voxel_value == 0 ? 0 : 1));
        }
        else if (k_debug_render == 2)
        {
            // @DEBUG: show transmittance as fluffy green ball.
            dest_image.Store(thread_idx.xy,
                            float4(0.5,
                                   0.75,
                                   0.25,
                                   1.0 - transmittance));
        }
    }
}
