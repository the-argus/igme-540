#include "Camera.h"
#include "Input.h"

using namespace DirectX;
using namespace ggp;

ggp::Camera::Camera(const Options& options) noexcept
	: m_projection(options.projection),
	m_sens(options.mouseSensitivity),
	m_fov(DegToRad(options.fovDegrees)),
	m_near(options.nearPlaneDistance),
	m_far(options.farPlaneDistance)
{
	XMVECTOR initialPos = XMLoadFloat3(&options.initialGlobalPosition);
	XMMATRIX viewMatrix = XMMatrixLookAtLH(
		initialPos,
		XMLoadFloat3(&options.initialTargetGlobalPosition),
		XMVectorSet(0.f, 1.f, 0.f, 0.f));
	XMStoreFloat4x4A(&m_viewMatrix, viewMatrix);
	XMVECTOR euler;
	XMVECTOR rotation;
	XMVECTOR unusedScale;
	// extract rotation/direction from the viewmatrix instead of calculating it ourselves
	XMMatrixDecompose(&unusedScale, &rotation, &unused, viewMatrix);
	
}

		const DirectX::XMFLOAT4A* GetViewMatrix() const noexcept;
		const DirectX::XMFLOAT4A* GetProjectionMatrix() const noexcept;

		void UpdateProjectionMatrix(f32 aspectRatio) noexcept;
		void Update(f32 dt) noexcept;

		void UpdateViewMatrix() noexcept;
