#language slang 2026

struct Vertex  // std430 aligned.
{
    float position_x;
    float position_y;
    float position_z;
    float normal_x;
    float normal_y;
    float normal_z;
    float uv_x;
    float uv_y;
};

struct Vertex_skin_data
{
    uint4  joint_mat_idxs;
    float4 weights;
};


cbuffer Uniforms
{
    StructuredBuffer<Vertex> static_combined_model;
    uint static_model_vertex_start;
    RWStructuredBuffer<Vertex> deformed_combined_model;
    uint deformed_model_vertex_start;

    uint num_vertices;

    StructuredBuffer<Vertex_skin_data> vertex_skin_datas;
    StructuredBuffer<float4x4> deform_mesh_joint_transforms;
}


[shader("compute")]
[numthreads(128, 1, 1)]
void compute_main(uint3 thread_idx : SV_DispatchThreadID,
                  uint3 local_thread_idx : SV_GroupThreadID)
{
    uint x = thread_idx.x;

    if (x < num_vertices)
    {
        const Vertex input_vertex = static_combined_model[static_model_vertex_start + x];
        Vertex* output_vertex = &deformed_combined_model[deformed_model_vertex_start + x];

        float3 input_pos       = float3(input_vertex.position_x,
                                        input_vertex.position_y,
                                        input_vertex.position_z);
        float3 input_norm      = float3(input_vertex.normal_x,
                                        input_vertex.normal_y,
                                        input_vertex.normal_z);
        float2 input_tex_coord = float2(input_vertex.uv_x,
                                        input_vertex.uv_y);
        uint4 joint            = vertex_skin_datas[x].joint_mat_idxs;
        float4 weight          = vertex_skin_datas[x].weights;

        // Compute deformed mesh.
        float4x4 deform_transform =
            (deform_mesh_joint_transforms[joint.x] * weight.x
             + deform_mesh_joint_transforms[joint.y] * weight.y
             + deform_mesh_joint_transforms[joint.z] * weight.z
             + deform_mesh_joint_transforms[joint.w] * weight.w);

        float3 output_position = float3(deform_transform * float4(input_pos, 1.0));
        float3 output_normal   = normalize(float3x3(deform_transform) * input_norm);

        // Spit out the data!
        output_vertex.position_x  = output_position.x;
        output_vertex.position_y  = output_position.y;
        output_vertex.position_z  = output_position.z;
        output_vertex.normal_x    = output_normal.x;
        output_vertex.normal_y    = output_normal.y;
        output_vertex.normal_z    = output_normal.z;
        output_vertex.uv_x        = input_tex_coord.x;
        output_vertex.uv_y        = input_tex_coord.y;
    }
}
