#include "Camera.h"
#include "Input.h"

#include <algorithm>

using namespace DirectX;
using namespace ggp;

ggp::Camera::Camera(const Options& options) noexcept
	: m_projection(options.projection),
	m_sens(options.mouseSensitivity),
	m_fov(DegToRad(options.fovDegrees)),
	m_near(options.nearPlaneDistance),
	m_far(options.farPlaneDistance)
{
	m_angles.x = std::clamp(options.initialRotation.x, -DegToRad(89), DegToRad(89));
	m_angles.y = fmodf(options.initialRotation.y, XM_2PI);
	m_transform.SetPosition(options.initialGlobalPosition);
	XMStoreFloat4x4(&m_viewMatrix, XMMatrixIdentity());
	UpdateProjectionMatrix(options.aspectRatio, options.width, options.height);
}

const DirectX::XMFLOAT4X4* ggp::Camera::GetViewMatrix() const noexcept
{
	return &m_viewMatrix;
}

const DirectX::XMFLOAT4X4* ggp::Camera::GetProjectionMatrix() const noexcept
{
	return &m_projectionMatrix;
}

void ggp::Camera::UpdateProjectionMatrix(f32 aspectRatio, u32 width, u32 height) noexcept
{
	gassert(is_aligned_to_type(&m_projectionMatrix));
	switch (m_projection)
	{
	case Projection::Perspective:
		XMStoreFloat4x4(&m_projectionMatrix, XMMatrixPerspectiveFovLH(m_fov, aspectRatio, m_near, m_far));
		break;
	case Projection::Orthographic:
		XMStoreFloat4x4(&m_projectionMatrix, XMMatrixOrthographicLH(f32(width), f32(height), m_near, m_far));
		break;
	}
}

void ggp::Camera::Update(f32 dt) noexcept
{
	if (Input::KeyPress('F'))
		m_isLocked = !m_isLocked;

	if (Input::MouseRightDown())
	{
		// orbit around a point 16 units in front of our face
		XMVECTOR delta = m_transform.LoadForward();
		delta = XMVectorMultiply(VectorSplat(16), delta);
		UpdateOrbital(dt, XMVectorAdd(m_transform.LoadPosition(), delta));
	}
	else if (!m_isLocked)
	{
		UpdateNoClip(dt);
	}

	UpdateViewMatrix();
}

void ggp::Camera::UpdateViewMatrix() noexcept
{
	XMVECTOR pos = m_transform.LoadPosition();
	XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 1.f);
	XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookToLH(pos, m_transform.LoadForward(), up));
}

void ggp::Camera::UpdateNoClip(f32 dt) noexcept
{
	// scroll to change speed
	m_moveSpeed = std::clamp(m_moveSpeed + Input::GetMouseWheel(), 0.f, 100.f);

	// move with WASD
	XMVECTOR direction = XMVectorSet(
		f32(Input::KeyDown('D')) - f32(Input::KeyDown('A')), 0.f,
		f32(Input::KeyDown('W')) - f32(Input::KeyDown('S')), 0.f);
	direction = XMVectorMultiply(direction, VectorSplat(m_moveSpeed * dt));
	m_transform.MoveRelativeVec(direction);

	direction = XMVectorSet(0.f, f32(Input::KeyDown(VK_SPACE)) - f32(Input::KeyDown(VK_SHIFT)), 0.f, 0.f);
	direction = XMVectorMultiply(direction, VectorSplat(m_moveSpeed * dt));
	m_transform.MoveAbsoluteLocalVec(direction);

	// rotate camera with mouse
	m_angles.x += Input::GetMouseYDelta() * m_sens;
	m_angles.y += Input::GetMouseXDelta() * m_sens;
	m_angles.x = std::clamp(m_angles.x, DegToRad(-89.f), DegToRad(89.f));
	m_angles.y = fmodf(m_angles.y, XM_2PI);
	m_transform.SetLocalEulerAngles({ m_angles.x, m_angles.y, 0.f });
}

void __vectorcall ggp::Camera::UpdateOrbital(f32 dt, DirectX::FXMVECTOR orbitCenter) noexcept
{}
