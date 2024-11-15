#include "forward_vertex_shader_input.hlsli"
#include "skybox_vertex_to_pixel.hlsli"

cbuffer ExternalData : register(b0)
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
}

SkyboxVertexToPixel main(VertexShaderInput input)
{
    SkyboxVertexToPixel output;
    float4x4 viewMatrixNoTranslation = viewMatrix;
    viewMatrixNoTranslation._14 = 0;
    viewMatrixNoTranslation._24 = 0;
    viewMatrixNoTranslation._34 = 0;
    
    output.position = mul(mul(projectionMatrix, viewMatrixNoTranslation), float4(input.position, 1.0f));
    // make sure z ends up as 1 after divide by w
    output.position.z = output.position.w;
    output.sampleDir = input.position;
	return output;
}