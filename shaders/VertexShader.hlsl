
struct VertexShaderInput
{ 
	float3 position	: POSITION;
	float3 normal	: NORMAL;
	float2 uv		: TEXCOORD;
};

struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float3 normal			: NORMAL;
	float2 uv				: TEXCOORD;
};

cbuffer ExternalData : register(b0)
{
	float4x4 world;
	float4x4 view;
	float4x4 projection;
}

VertexToPixel main( VertexShaderInput input )
{
	VertexToPixel output;

	matrix wvp = mul(projection, mul(view, world));
	output.screenPosition = mul(wvp, float4(input.position, 1.0f));
	output.normal = input.normal;
	output.uv = input.uv;

	// to pixel shader
	return output;
}