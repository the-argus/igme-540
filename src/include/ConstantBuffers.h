#pragma once

#include <DirectXMath.h> 

namespace ggp::cb
{
	struct OffsetAndColor
	{
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT3 offset;
	};

	struct TransformAndColor
	{
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT4X4 worldMatrix;
	};
}