#include "forward_vertex_to_pixel.hlsli"

struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float3 normal			: NORMAL;
	float2 uv				: TEXCOORD;
};

cbuffer ExternalData : register(b0)
{
    float4 colorTint;
    float totalTime;
}

float random(float2 s)
{
    return frac(sin(dot(s, float2(12.9898, 78.233))) * 43758.5453123);
}

float4 main(VertexToPixel input) : SV_TARGET
{
    return colorTint;
}