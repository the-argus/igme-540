#include "forward_vertex_to_pixel.hlsli"
#include "forward_vertex_shader_input.hlsli"

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
    output.tangent = mul((float3x3) world, input.tangent);
    output.uv = input.uv;

	// to pixel shader
    return output;
}