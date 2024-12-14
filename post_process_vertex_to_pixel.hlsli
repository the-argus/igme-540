#ifndef __GGP_POST_PROCESS_VERTEX_TO_PIXEL_HLSL_H__
#define __GGP_POST_PROCESS_VERTEX_TO_PIXEL_HLSL_H__

struct PostProcessVertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

#endif