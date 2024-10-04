#pragma once
#include <DirectXMath.h>
#include "Transform.h"
#include "ggp_math.h"

namespace ggp
{
	enum class Projection : u8
	{
		Perspective,
		Orthographic,
	};

	class Camera
	{
	public:
		Camera() = delete;
		struct Options
		{
			Projection projection = Projection::Perspective;
			f32 aspectRatio = 16.f / 9.f;
			f32 mouseSensitivity = 1.f;
			f32 fovDegrees = 90.f;
			f32 nearPlaneDistance = 0.1f;
			f32 farPlaneDistance = 1000.f;
			DirectX::XMFLOAT3 initialGlobalPosition;
			DirectX::XMFLOAT3 initialTargetGlobalPosition;
		};

		Camera(const Options&) noexcept;

		const DirectX::XMFLOAT4A* GetViewMatrix() const noexcept;
		const DirectX::XMFLOAT4A* GetProjectionMatrix() const noexcept;

		void UpdateProjectionMatrix(f32 aspectRatio) noexcept;
		void Update(f32 dt) noexcept;

	private:
		void UpdateViewMatrix() noexcept;

		DirectX::XMFLOAT4X4A m_viewMatrix;
		DirectX::XMFLOAT4X4A m_projectionMatrix;
		Transform m_transform = Transform::Create();
		Projection m_projection = Projection::Perspective;
		f32 m_sens;
		f32 m_fov;
		f32 m_near;
		f32 m_far;
	};
}