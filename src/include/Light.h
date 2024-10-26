#pragma once

/*
    Everything here must be matched by light.hlsli
*/

#include <DirectXMath.h>

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
		DirectX::XMFLOAT2 _padding;
	};
}