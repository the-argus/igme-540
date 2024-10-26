#include "forward_vertex_to_pixel.hlsli"

struct VertexShaderInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer ExternalData : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4x4 worldInverseTranspose;
}

ForwardVertexToPixel main(VertexShaderInput input)
{
    ForwardVertexToPixel output;

    float4x4 wvp = mul(projection, mul(view, world));
    output.screenPosition = mul(wvp, float4(input.position, 1.0f));
    output.worldPosition = mul(world, float4(input.position, 1.0f)).xyz;
    output.normal = mul((float3x3) worldInverseTranspose, input.normal);
    output.uv = input.uv;

	// to pixel shader
    return output;
}