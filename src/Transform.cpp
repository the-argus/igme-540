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

const XMFLOAT4X4* ggp::Transform::GetMatrix() noexcept
{
	return hierarchy->GetMatrix(handle);
}
