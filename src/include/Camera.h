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

		// copy/move unimplemented
		Camera(const Camera&) = delete;
		Camera& operator=(const Camera&) = delete;
		Camera(Camera&&) = delete;
		Camera& operator=(Camera&&) = delete;

		struct Options
		{
			Projection projection = Projection::Perspective;
			u32 width;
			u32 height;
			f32 aspectRatio;
			f32 mouseSensitivity = 0.01f;
			f32 fovDegrees = 90.f;
			f32 nearPlaneDistance = 0.1f;
			f32 farPlaneDistance = 1000.f;
			DirectX::XMFLOAT3 initialGlobalPosition = { 0.f, 0.f, 0.f };
			DirectX::XMFLOAT2 initialRotation = { DirectX::XM_PI/2.f, 0.f };
		};

		Camera(const Options&) noexcept;

		const DirectX::XMFLOAT4X4* GetViewMatrix() const noexcept;
		const DirectX::XMFLOAT4X4* GetProjectionMatrix() const noexcept;

		void UpdateProjectionMatrix(f32 aspectRatio, u32 width, u32 height) noexcept;
		void Update(f32 dt) noexcept;

		inline Transform GetTransform() noexcept { return m_transform; }
		inline f32 GetFov() noexcept { return m_fov; }

	private:
		void UpdateViewMatrix() noexcept;

		// types of camera movement
		void UpdateNoClip(f32 dt) noexcept;
		void __vectorcall UpdateOrbital(f32 dt, DirectX::FXMVECTOR orbitCenter) noexcept;

		DirectX::XMFLOAT4X4 m_viewMatrix;
		DirectX::XMFLOAT4X4 m_projectionMatrix;
		Transform m_transform = Transform::Create();
		Projection m_projection = Projection::Perspective;
		f32 m_sens;
		f32 m_fov;
		f32 m_near;
		f32 m_far;
		f32 m_moveSpeed = 1.f;
		DirectX::XMFLOAT2 m_angles;
	};
}