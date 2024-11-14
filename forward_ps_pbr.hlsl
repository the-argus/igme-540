#include "forward_vertex_to_pixel.hlsli"
#include "light.hlsli"
#include "phong.hlsli"

cbuffer ExternalData : register(b0)
{
    float4 colorTint;
    float4 ambient;
    float3 cameraPosition;
    float2 uvScale;
    float2 uvOffset;
    float totalTime;
    float roughness;

    Light lights[MAX_LIGHTS];
}

SamplerState basicSampler : register(s0);
Texture2D surfaceColorTexture : register(t0);
Texture2D specularTexture : register(t1);
// Texture2D normalTexture : register(t2);

float4 main(ForwardVertexToPixel input) : SV_TARGET
{
    input.normal = normalize(input.normal);
    input.tangent = normalize(input.tangent);
    input.uv += uvOffset;
    input.uv *= uvScale;
    const float3 surfaceColor = (surfaceColorTexture.Sample(basicSampler, input.uv) * colorTint).rgb;
    const float specularStrength = specularTexture.Sample(basicSampler, input.uv).r;
   
    // perform normal mapping on input normal
    {
        const float3 unpackedNormal = float3(0, 0, 1); // normalize(normalTexture.Sample(basicSampler, input.uv).rgb * 2 - 1);
        // grandt-schmidt orthonormalize against normal
        const float3 T = normalize(input.tangent - input.normal * dot(input.tangent, input.normal));
        const float3 B = cross(T, input.normal);
        const float3x3 TBN = float3x3(T, B, input.normal);
        input.normal = mul(TBN, unpackedNormal);
    }
    
    float3 total = ambient.rgb * surfaceColor;
    const float3 dirToCamera = normalize(cameraPosition - input.worldPosition);
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        const Light light = lights[i];
        const float realIntensity = LightAttenuation(light, input.worldPosition) * light.intensity;
        const float3 dirToLight = LightDirection(light, input.worldPosition);
       
        // phong specular calculation
        float3 specularTerm;
        {
            const float3 reflectedLightDir = reflect(dirToLight, input.normal);
            const float viewToReflectionSimilarity = saturate(dot(reflectedLightDir, dirToCamera));
            float exponent = (1.f - roughness) * MAX_SPECULAR_EXPONENT;
            if (exponent <= 0.f) // clamp
            {
                exponent = 0.0001f;
            }
        
            specularTerm =
                pow(viewToReflectionSimilarity, exponent) * // Initial specular term
                light.color * realIntensity * // Colored by light’s color
                specularStrength * // scaled by specular map
                surfaceColor; // Tinted by surface color
        }

        const float3 diffuseColor = saturate(dot(input.normal, -dirToLight)) * light.color * realIntensity * surfaceColor;

        total += diffuseColor + specularTerm;
    }
    return float4(total, 1.f);
}
