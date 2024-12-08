#pragma once

/*
    Everything here must be matched by light.hlsli
*/

#include <DirectXMath.h>

#include "Graphics.h"
#include "short_numbers.h"

#define LIGHT_TYPE_DIRECTIONAL (0)
#define LIGHT_TYPE_POINT (1)
#define LIGHT_TYPE_SPOT (2)

#define MAX_LIGHTS (5)

namespace ggp
{
	struct Light
	{
		i32 type;
		DirectX::XMFLOAT3 direction;
		f32 range;
		DirectX::XMFLOAT3 position;
		f32 intensity;
		DirectX::XMFLOAT3 color;
		f32 spotInnerAngleRadians;
		f32 spotOuterAngleRadians;
		i32 isShadowCaster;
		f32 _padding;
		DirectX::XMFLOAT4X4 shadowView;
		DirectX::XMFLOAT4X4 shadowProjection;
	};

	struct ShadowMapResources
	{
		ID3D11ShaderResourceView* shaderResourceView;
		ID3D11DepthStencilView* depthStencilView;
	};
}