#include "Transform.h"

using namespace DirectX;

ggp::TransformHierarchy* ggp::internals::hierarchy = nullptr;
using namespace ggp::internals;

auto ggp::Transform::CreateHierarchySingleton() noexcept -> TransformHierarchy*
{
	gassert(!hierarchy, "already initialized transform hierarchy");
	hierarchy = new TransformHierarchy(); // NOTE: could throw :/
	return hierarchy;
}

void ggp::Transform::DestroyHierarchySingleton(TransformHierarchy** ptr) noexcept
{
	gassert(hierarchy);
	gassert(hierarchy == *ptr);
	delete hierarchy;
	hierarchy = nullptr;
	*ptr = nullptr;
}

auto ggp::Transform::Create() noexcept -> Transform
{
	return hierarchy->InsertTransform();
}

auto ggp::Transform::GetFirstChild() noexcept -> std::optional<Transform>
{
	return hierarchy->GetFirstChild(handle);
}

auto ggp::Transform::GetNextSibling() noexcept -> std::optional<Transform>
{
	return hierarchy->GetNextSibling(handle);
}
	
auto ggp::Transform::GetParent() noexcept -> std::optional<Transform>
{
	return hierarchy->GetParent(handle);
}


auto ggp::Transform::AddChild() noexcept -> Transform
{
	return hierarchy->AddChild(handle);
}

void ggp::Transform::Destroy() noexcept
{
	return hierarchy->Destroy(handle);
}

const XMFLOAT4X4* ggp::Transform::GetWorldMatrixPtr() noexcept
{
	return hierarchy->GetWorldMatrixPtr(handle);
}

const XMFLOAT4X4* ggp::Transform::GetWorldInverseTransposeMatrixPtr() noexcept
{
	return hierarchy->GetWorldInverseTransposeMatrixPtr(handle);
}

// trivial getters and setters
DirectX::XMFLOAT4X4 ggp::Transform::GetWorldMatrix() { return *GetWorldMatrixPtr(); }
DirectX::XMFLOAT4X4 ggp::Transform::GetWorldInverseTransposeMatrix() { return *GetWorldInverseTransposeMatrixPtr(); }
void ggp::Transform::SetPosition(float x, float y, float z) { SetPosition({x, y, z}); }
void ggp::Transform::SetPosition(DirectX::XMFLOAT3 position) { hierarchy->SetLocalPosition(handle, position); }
void ggp::Transform::SetRotation(float pitch, float yaw, float roll) { SetRotation({ pitch, yaw, roll }); }
void ggp::Transform::SetRotation(DirectX::XMFLOAT3 rotation) { hierarchy->SetLocalRotation(handle, rotation); }
void ggp::Transform::SetScale(float x, float y, float z) { SetScale({ x, y, z }); }
void ggp::Transform::SetScale(DirectX::XMFLOAT3 scale) { hierarchy->SetLocalScale(handle, scale); }
DirectX::XMFLOAT3 ggp::Transform::GetPosition() const { return hierarchy->GetLocalPosition(handle); }
DirectX::XMFLOAT3 ggp::Transform::GetPitchYawRoll() const { return hierarchy->GetLocalRotation(handle); }
DirectX::XMFLOAT3 ggp::Transform::GetScale() const { return hierarchy->GetLocalScale(handle); }

// transformers
void ggp::Transform::MoveAbsolute(float x, float y, float z) { MoveAbsoluteVec(XMVectorSet(x, y, z, 0)); }
void ggp::Transform::MoveAbsolute(DirectX::XMFLOAT3 offset) { MoveAbsoluteVec(XMLoadFloat3(&offset)); }
void ggp::Transform::MoveAbsoluteLocal(float x, float y, float z) { MoveAbsoluteLocalVec(XMVectorSet(x, y, z, 0.f)); }
void ggp::Transform::MoveAbsoluteLocal(DirectX::XMFLOAT3 offset) { MoveAbsoluteLocalVec(XMLoadFloat3(&offset)); }
void ggp::Transform::Rotate(float pitch, float yaw, float roll) { RotateVec(XMVectorSet(pitch, yaw, roll, 0.f)); }
void ggp::Transform::Rotate(DirectX::XMFLOAT3 rotation) { RotateVec(XMLoadFloat3(&rotation)); }
void ggp::Transform::Scale(float x, float y, float z) { ScaleVec(XMVectorSet(x, y, z, 0)); }
void ggp::Transform::Scale(DirectX::XMFLOAT3 scale) { ScaleVec(XMLoadFloat3(&scale)); }
void ggp::Transform::MoveRelative(float x, float y, float z) { MoveRelativeVec(XMVectorSet(x, y, z, 0.f)); }
void ggp::Transform::MoveRelative(XMFLOAT3 offset) { MoveRelativeVec(XMLoadFloat3(&offset)); }

XMFLOAT3 ggp::Transform::GetForward() const
{
	XMFLOAT3 out;
	XMVECTOR forward = LoadForwardVector();
	XMStoreFloat3(&out, forward);
	return out;
}

XMFLOAT3 ggp::Transform::GetUp() const
{
	XMFLOAT3 out;
	XMVECTOR forward = LoadUpVector();
	XMStoreFloat3(&out, forward);
	return out;
}

XMFLOAT3 ggp::Transform::GetRight() const
{
	XMFLOAT3 out;
	XMVECTOR forward = LoadRightVector();
	XMStoreFloat3(&out, forward);
	return out;
}
