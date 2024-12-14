#include "post_process_vertex_to_pixel.hlsli"

cbuffer externalData : register(b0)
{
    int blurRadius;
    float pixelWidth;
    float pixelHeight;
}

Texture2D gameRenderTarget : register(t0);
SamplerState postProcessSampler : register(s0);

float4 main(PostProcessVertexToPixel input) : SV_TARGET
{
    float4 total = 0;
    int sampleCount = 0;
    for (int x = -blurRadius; x <= blurRadius; x++)
    {
        for (int y = -blurRadius; y <= blurRadius; y++)
        {
            float2 uv = input.uv;
            uv += float2(x * pixelWidth, y * pixelHeight);
            total += gameRenderTarget.Sample(postProcessSampler, uv);
            sampleCount++;
        }
    }
    // average
    return total / sampleCount;
}