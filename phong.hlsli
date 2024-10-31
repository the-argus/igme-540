#ifndef __GGP_PHONG_SHADER_FUNCS__
#define __GGP_PHONG_SHADER_FUNCS__

#include "light.hlsli"

// TODO: move these functions or rename file, this aint phong

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

#endif