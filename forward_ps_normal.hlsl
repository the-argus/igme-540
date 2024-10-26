#include "forward_vertex_to_pixel.hlsli"

cbuffer ExternalData : register(b0)
{
    float4 colorTint;
    float3 cameraPosition;
    float totalTime;
    float roughness;
    float ambient;
}

float4 main(ForwardVertexToPixel input) : SV_TARGET
{
    return float4(input.normal, 1);
}
