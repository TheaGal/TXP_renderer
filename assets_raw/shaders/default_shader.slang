#language slang 2026


// Uniform data to be passed from application -> shader.
cbuffer Uniforms
{
    float4x4 model_view_projection;
}

/// Per-vertex attributes from vertex buffer.
struct Assembled_vertex
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : UV;
};

/// Output of the vertex shader, and input to the fragment shader.
struct Coarse_vertex
{
    float2 uv;
};


struct Vertex_stage_output
{
    Coarse_vertex coarse_vertex : CoarseVertex;
    float4        sv_position   : SV_Position;
};

[shader("vertex")]
Vertex_stage_output vert_main(Assembled_vertex assembled_vertex)
{
    Vertex_stage_output output;

    float3 position = assembled_vertex.position;
    float2 uv       = assembled_vertex.uv;

    output.coarse_vertex.uv = uv;
    output.sv_position = mul(model_view_projection, float4(position, 1.0));

    return output;
}

[shader("fragment")]
float4 frag_main(Coarse_vertex coarse_vertex : CoarseVertex) : SV_Target
{
    return float4(1, 0, 0, 1);
}
