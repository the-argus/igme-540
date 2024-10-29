#include "forward_vertex_to_pixel.hlsli"
#include "light.hlsli"
#include "phong.hlsli"

cbuffer ExternalData : register(b0)
{
    float4 colorTint;
    float4 ambient;
    float3 cameraPosition;
    float totalTime;
    float roughness;

    Light lights[MAX_LIGHTS];
}

SamplerState basicSampler : register(s0);
Texture2D surfaceColorTexture : register(t0);

float4 main(ForwardVertexToPixel input) : SV_TARGET
{
    input.normal = normalize(input.normal);
    float3 total = ambient.rgb;
    float3 dirToCamera = normalize(cameraPosition - input.worldPosition);
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        const Light light = lights[i];
        const float realIntensity = LightAttenuation(light, input.worldPosition) * light.intensity;
        const float3 dirToLight = LightDirection(light, input.worldPosition);

        float3 specColor = PhongSpecular(light, input.worldPosition, input.normal, colorTint, roughness, realIntensity, dirToCamera);

        float3 ambientColor = ambient.rgb * colorTint.rgb;

        float3 diffuseColor = saturate(dot(input.normal, -dirToLight)) * light.color * realIntensity * colorTint.rgb;

        total += diffuseColor + specColor + ambientColor;
    }
    return float4(total, 1.f);
}
