#include "skybox_vertex_to_pixel.hlsli"

TextureCube skybox: register(t0);
SamplerState skyboxSampler: register(s0);

float4 main(SkyboxVertexToPixel input) : SV_TARGET
{
    return skybox.Sample(skyboxSampler, input.sampleDir);
}