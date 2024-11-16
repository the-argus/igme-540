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
    int useSpecular;

    Light lights[MAX_LIGHTS];
}

SamplerState textureSampler : register(s0);
Texture2D albedoTexture : register(t0);
Texture2D specularTexture : register(t1);
Texture2D normalTexture : register(t2);

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
 
    const float3 surfaceColor = (albedoTexture.Sample(textureSampler, input.uv) * colorTint).rgb;
    const float specularStrength = useSpecular ? specularTexture.Sample(textureSampler, input.uv).r : 1.f;
    
    float3 totalLight = ambient.rgb * surfaceColor;
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
       
        // no specular if diffuse is 0
        specularTerm *= any(diffuseColor);

        totalLight += diffuseColor + specularTerm;
    }
    return float4(totalLight, 1.f);
}
