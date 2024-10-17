#pragma once

#include <DirectXMath.h>

#include "SimpleShader.h"

namespace ggp
{
	struct Material
	{
		DirectX::XMFLOAT4 color;
		SimpleVertexShader* vertexShader;
		SimplePixelShader* pixelShader;
	};
}
