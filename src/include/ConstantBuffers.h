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

	struct WVPAndColor
	{
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT4X4 worldMatrix;
		DirectX::XMFLOAT4X4 viewMatrix;
		DirectX::XMFLOAT4X4 projectionMatrix;
	};
}