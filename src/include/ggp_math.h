#pragma once

#include <DirectXMath.h>

namespace ggp
{
	inline DirectX::XMVECTOR VectorSplat(f32 f)
	{
		return DirectX::XMVectorSet(f, f, f, f);
	}

	inline constexpr float DegToRad(float deg)
	{
		return (DirectX::XM_PI / 180.f) * deg;
	}

	inline DirectX::XMFLOAT3 QuatToEuler(const DirectX::XMFLOAT4& q) noexcept
	{
		float xx = q.x * q.x;
		float yy = q.y * q.y;
		float zz = q.z * q.z;

		float m31 = 2.f * q.x * q.z + 2.f * q.y * q.w;
		float m32 = 2.f * q.y * q.z - 2.f * q.x * q.w;
		float m33 = 1.f - 2.f * xx - 2.f * yy;

		float cy = sqrtf(m33 * m33 + m31 * m31);
		float cx = atan2f(-m32, cy);
		if (cy > 16.f * FLT_EPSILON)
		{
			float m12 = 2.f * q.x * q.y + 2.f * q.z * q.w;
			float m22 = 1.f - 2.f * xx - 2.f * zz;

			return DirectX::XMFLOAT3{cx, atan2f(m31, m33), atan2f(m12, m22)};
		}
		else
		{
			float m11 = 1.f - 2.f * yy - 2.f * zz;
			float m21 = 2.f * q.x * q.y - 2.f * q.z * q.w;

			return DirectX::XMFLOAT3{ cx, 0.f, atan2f(-m21, m11)};
		}
	}

	// adapted from https://stackoverflow.com/questions/60350349/directx-get-pitch-yaw-roll-from-xmmatrix
	inline DirectX::XMVECTOR ExtractEulersFromMatrix(const DirectX::XMFLOAT4X4A* matrix) noexcept
	{
		// TODO: idk if this works when the matrix is a combined SRT
		return DirectX::XMVectorSet(
			float(::asin(-matrix->_23)),
			float(::atan2(matrix->_13, matrix->_33)),
			float(::atan2(matrix->_21, matrix->_22)),
			0.f);
	}
}