#ifndef __GGP_FORWARD_VERTEX_SHADER_INPUT__
#define __GGP_FORWARD_VERTEX_SHADER_INPUT__

struct VertexShaderInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

#endif