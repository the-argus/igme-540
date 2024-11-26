#ifndef __GGP_LIGHT__
#define __GGP_LIGHT__
/*
    Everything here must be matched by Light.h
*/

#define MAX_SPECULAR_EXPONENT (256.0f)

#define LIGHT_TYPE_DIRECTIONAL (0)
#define LIGHT_TYPE_POINT (1)
#define LIGHT_TYPE_SPOT (2)

#define MAX_LIGHTS (5)
	
struct Light
{
    int type;
    float3 direction;
    float range;
    float3 position;
    float intensity;
    float3 color;
    float spotInnerAngleRadians;
    float spotOuterAngleRadians;
    float2 _padding;
};

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