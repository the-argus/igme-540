#pragma once

#include <DirectXMath.h>

namespace ggp
{
	class Transform
	{
	public:
		Transform() noexcept;
	private:
		DirectX::XMFLOAT3A m_localPosition;
		DirectX::XMFLOAT3A m_localRotation;
		DirectX::XMFLOAT4A m_quaternion;
		DirectX::XMFLOAT3A m_localScale;
	};

	class TransformHierarchy
	{
	};
}
