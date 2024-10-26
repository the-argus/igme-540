#ifndef __GGP_PHONG_SHADER_FUNCS__
#define __GGP_PHONG_SHADER_FUNCS__

#include "light.hlsli"

float3 LightDirection(Light light, float3 worldPosition)
{
    switch (light.type)
    {
        case LIGHT_TYPE_DIRECTIONAL:
                return -light.direction;
        default:
                return normalize(light.position - worldPosition);
    }
}

float LightAttenuation(Light light, float3 worldPosition)
{
    if (light.type == LIGHT_TYPE_DIRECTIONAL)
    {
        return 1.f;
    }
   
    float attenuationFactor = 0.f;
    float dist = distance(light.position, worldPosition);
    attenuationFactor = saturate(1.0f - (dist * dist) / (light.range * light.range));
    attenuationFactor *= attenuationFactor;
    return attenuationFactor;
}

float3 PhongSpecular(Light light, float3 worldPosition, float3 normal, float4 diffuseColor, float roughness, float intensity, float3 dirToCamera)
{
    float3 direction = float3(0.f, 1.f, 0.f);

    float3 refl = reflect(direction, normal);
    float RdotV = saturate(dot(refl, dirToCamera));
    float exponent = (1.f - roughness) * MAX_SPECULAR_EXPONENT;
    if (exponent <= 0.f) // clamp
    {
        exponent = 0.0001f;
    }
    
    float3 specularTerm =
        pow(RdotV, exponent) * // Initial specular term
        light.color * intensity * // Colored by light’s color
        diffuseColor.rgb; // Tinted by surface color

    return specularTerm;
}

#endif