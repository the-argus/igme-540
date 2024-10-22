#include "forward_vertex_to_pixel.hlsli"

cbuffer ExternalData : register(b0)
{
    float4 colorTint;
    float totalTime;
}

float4 main(ForwardVertexToPixel input) : SV_TARGET
{
    return float4(input.uv, 1, 0);
}
