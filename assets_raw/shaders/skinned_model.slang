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

struct Vertex_collection
{
    Vertex verts[];
};

struct Vertex_skin_data
{
    uint4  joint_mat_idxs;
    float4 weights;
};

struct Vertex_skin_data_collection
{
    Vertex_skin_data vertex_skin_datas[];
};

struct Deform_mesh_joint_transform_collection
{
    float4x4 transforms[];
};


cbuffer Uniforms
{
    RWStructuredBuffer<Vertex> deformed_combined_model;
}


[shader("compute")]
[numthreads(128, 1, 1)]
void compute_main(uniform Vertex_collection* input_model,
                  uniform Vertex_skin_data_collection* skin_data_coll,
                  uniform Deform_mesh_joint_transform_collection* joint_trans_coll,
                  uniform uint input_model_vertex_start,
                  uniform uint output_model_vertex_start,
                  uniform uint num_model_vertices,
                  uint3 thread_idx : SV_DispatchThreadID,
                  uint3 local_thread_idx : SV_GroupThreadID)
{
    uint x = thread_idx.x;
    if (x < num_model_vertices)
    {
        const Vertex input_vertex = input_model->verts[input_model_vertex_start + x];
        Vertex* output_vertex = &deformed_combined_model[output_model_vertex_start + x];

        float3 input_pos       = float3(input_vertex.position_x,
                                        input_vertex.position_y,
                                        input_vertex.position_z);
        float3 input_norm      = float3(input_vertex.normal_x,
                                        input_vertex.normal_y,
                                        input_vertex.normal_z);
        float2 input_tex_coord = float2(input_vertex.uv_x,
                                        input_vertex.uv_y);
        uint4 joint            = skin_data_coll->vertex_skin_datas[x].joint_mat_idxs;
        float4 weight          = skin_data_coll->vertex_skin_datas[x].weights;

        // Compute deformed mesh.
        float4x4 deform_transform =
            (joint_trans_coll->transforms[joint.x] * weight.x
             + joint_trans_coll->transforms[joint.y] * weight.y
             + joint_trans_coll->transforms[joint.z] * weight.z
             + joint_trans_coll->transforms[joint.w] * weight.w);

        float3 output_position = mul(deform_transform, float4(input_pos, 1.0)).xyz;
        float3 output_normal   = normalize(mul(float3x3(deform_transform), input_norm));

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
