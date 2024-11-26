#include "forward_vertex_to_pixel.hlsli"
#include "light.hlsli"
#include "pbr.hlsli"

cbuffer ExternalData : register(b0)
{
    float4 colorTint;
    float3 cameraPosition;
    float2 uvScale;
    float2 uvOffset;
    float totalTime;

    // if this is on, use roughness float instead of texture
    int useFlatRoughness;
    float roughness;

    Light lights[MAX_LIGHTS];
}

SamplerState textureSampler : register(s0);
Texture2D albedoTexture : register(t0);
Texture2D roughnessTexture : register(t1);
Texture2D normalTexture : register(t2);
Texture2D metalnessTexture : register(t3);

float3 GammaUnCorrectAlbedoTexture(float3 sampled)
{
    return pow(sampled, 2.2f);
}

float4 main(ForwardVertexToPixel input) : SV_TARGET
{
    input.uv += uvOffset;
    input.uv *= uvScale;
   
    // perform normal mapping on input normal
    {
        const float3 unpackedNormal = normalize(normalTexture.Sample(textureSampler, input.uv).rgb * 2 - 1);
        const float3 N = normalize(input.normal);
        float3 T = normalize(input.tangent);
        // grandt-schmidt orthonormalize against normal
        T = normalize(T - N * dot(T, N));
        const float3 B = cross(T, N);
        const float3x3 TBN = float3x3(T, B, N);
        input.normal = mul(unpackedNormal, TBN);
    }
 
    const float3 surfaceColor = (GammaUnCorrectAlbedoTexture(albedoTexture.Sample(textureSampler, input.uv).rgb) * colorTint.rgb);
    const float trueRoughness = useFlatRoughness ? roughness : roughnessTexture.Sample(textureSampler, input.uv).r;
    const float metalness = metalnessTexture.Sample(textureSampler, input.uv).r;
    const float3 specularColor = lerp(F0_NON_METAL, surfaceColor.rgb, metalness);

    float3 totalLight = float3(0.f, 0.f, 0.f);
    const float3 dirToCamera = normalize(cameraPosition - input.worldPosition);
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        const Light light = lights[i];
        const float trueIntensity = LightAttenuation(light, input.worldPosition) * light.intensity;
        const float3 dirToLight = LightDirection(light, input.worldPosition);
        
        float3 fresnel;
        const float3 specularTerm = MicrofacetBRDF(input.normal, dirToLight, dirToCamera, trueRoughness, specularColor, fresnel);
        const float3 diffuseTerm = DiffusePBR(input.normal, dirToLight);
        const float3 balancedDiff = DiffuseEnergyConserve(diffuseTerm, fresnel, metalness);
        totalLight += (balancedDiff * surfaceColor + specularTerm) * trueIntensity * light.color;
    }
    return float4(pow(totalLight, 1.0f / 2.2f), 1.f);
}
