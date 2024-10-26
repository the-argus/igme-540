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

#endif