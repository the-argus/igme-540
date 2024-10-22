#include "forward_vertex_to_pixel.hlsli"

cbuffer ExternalData : register(b0)
{
    float4 colorTint;
    float totalTime;
}

/*
    Hash, Noise, and OctavedNoise (originally SeaOctave) functions taken mostly from https://www.shadertoy.com/view/Ms2SD1
    Assuming these functions were written by the author, https://www.shadertoy.com/user/TDM
    Some minor fixes made by yours truly (Ian McFarlane) to port to hlsl
*/

float Hash(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453123);
}

float Noise(in float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    return -1.0 + 2.0 * lerp(lerp(Hash(i + float2(0.0, 0.0)),
                     Hash(i + float2(1.0, 0.0)), u.x),
                lerp(Hash(i + float2(0.0, 1.0)),
                     Hash(i + float2(1.0, 1.0)), u.x), u.y);
}

float OctavedNoise(float2 uv, float octave)
{
    uv += Noise(uv); // noise() function not in hlsl 5 :(
    float2 wv = 1.0 - abs(sin(uv));
    float2 swv = abs(cos(uv));
    wv = lerp(wv, swv, wv);
    return pow(1.0 - pow(wv.x * wv.y, 0.65), octave);
}

float FunnyNoise(float2 coord)
{
    const float OCTAVES = 3;
    const float SPEED = 0.5f;
    
    float freq = 10.f;
    float amp = 0.2;
    
    float accumulatedNoise = 0.f;
    float totalAmp = 0.f;
    
    for (int i = 0; i < OCTAVES; i++)
    {
        float time = 1 + totalTime * SPEED;
        float noise = OctavedNoise((coord + time) * freq, i);

        accumulatedNoise += noise * amp;
        totalAmp += amp;

        amp *= 0.5;
        freq *= 0.5;
    }

    return accumulatedNoise / (totalAmp * 1.5f);
}

float4 main(ForwardVertexToPixel input) : SV_TARGET
{
    float rvalue = FunnyNoise(input.uv);
    float gvalue = FunnyNoise(float2(rvalue, rvalue));
    float bvalue = FunnyNoise(float2(rvalue, gvalue));
    return float4(rvalue, gvalue, bvalue, 0);
}
