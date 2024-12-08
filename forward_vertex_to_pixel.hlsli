#ifndef __GGP_FORWARD_RASTERIZED_DATA_HLSL_H__
#define __GGP_FORWARD_RASTERIZED_DATA_HLSL_H__
/*
Holds the struct which contains data passed through the rasterizer (vertex to pixel)
in a forward rendering mode
*/

struct ForwardVertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float3 normal			: NORMAL;
    float3 tangent			: TANGENT;
    float3 worldPosition    : WORLD_POSITION;
	float2 uv				: TEXCOORD;
    float4 shadowMapPos     : SHADOW_POSITION;
};

#endif