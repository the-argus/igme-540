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
void ggp::Transform::MoveAbsolute(float x, float y, float z)
{
	XMVECTOR diff = XMVectorSet(x, y, z, 0);
	XMVECTOR current = LoadLocalPosition();
	StoreLocalPosition(XMVectorAdd(diff, current));
}

void ggp::Transform::MoveAbsolute(DirectX::XMFLOAT3 offset)
{
	XMVECTOR diff = XMLoadFloat3(&offset);
	XMVECTOR current = LoadLocalPosition();
	StoreLocalPosition(XMVectorAdd(diff, current));
}

void ggp::Transform::Rotate(float pitch, float yaw, float roll)
{
	// not having multiple copies of the rotate function like
	// move, since its pretty complicated and i think i may have got it wrong
	Rotate({ pitch, yaw, roll });
}

void ggp::Transform::Rotate(DirectX::XMFLOAT3 rotation)
{
	XMVECTOR diff = XMLoadFloat3(&rotation);
	XMVECTOR current = LoadLocalEulerAngles();
	// convert to quat
	current = XMQuaternionRotationRollPitchYawFromVector(current);
	diff = XMQuaternionRotationRollPitchYawFromVector(diff);

	// store, quattoeuler doesnt use simd
	XMFLOAT4 q;
	current = XMQuaternionMultiply(diff, current);
	XMStoreFloat4(&q, current);

	SetRotation(QuatToEuler(q));
}

void ggp::Transform::Scale(float x, float y, float z)
{
	XMVECTOR diff = XMVectorSet(x, y, z, 0);
	XMVECTOR current = LoadLocalScale();
	StoreLocalScale(XMVectorMultiply(diff, current));
}

void ggp::Transform::Scale(DirectX::XMFLOAT3 scale)
{
	XMVECTOR diff = XMLoadFloat3(&scale);
	XMVECTOR current = LoadLocalScale();
	StoreLocalScale(XMVectorMultiply(diff, current));
}

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
