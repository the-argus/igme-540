#ifndef __GPP_SKYBOX_VERTEX_TO_PIXEL_HLSL_H__
#define __GPP_SKYBOX_VERTEX_TO_PIXEL_HLSL_H__

struct SkyboxVertexToPixel
{
	float4 position			: SV_POSITION;
	float3 sampleDir		: DIRECTION;
};

#endif