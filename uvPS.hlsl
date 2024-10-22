struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float3 normal			: NORMAL;
	float2 uv				: TEXCOORD;
};

cbuffer ExternalData : register(b0)
{
    float4 colorTint;
    float totalTime;
}

float4 main(VertexToPixel input) : SV_TARGET
{
    return float4(input.uv, 1, 0);
}
