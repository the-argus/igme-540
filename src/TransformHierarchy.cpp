
#include "TransformHierarchy.h"
#include "memutils.h"

using namespace DirectX;

ggp::TransformHierarchy::TransformHierarchy() noexcept :m_transformAllocator(BlockAllocator::Options{
	.maxBytes = 10_MB,
		.initialBytes = 1_MB,
		.blockSize = sizeof(InternalTransform),
		.minimumAlignmentExponent = alignment_exponent(alignof(InternalTransform))
})
{
}

auto ggp::TransformHierarchy::InsertTransform() noexcept -> Handle
{
	auto* ptr = m_transformAllocator.Create<InternalTransform>();
	return Handle(m_transformAllocator.GetIndexFromPointer(ptr));
}

void ggp::TransformHierarchy::Destroy(Handle handle) noexcept
{
	auto* trans = (InternalTransform*)m_transformAllocator.GetPointerFromIndex(handle._inner);
	if (trans->parentHandle == -1) // we are in root transforms, need to remove from that so we dont get freed @ the end
	{
		auto rootPosition = std::find(m_rootTransforms.begin(), m_rootTransforms.end(), handle._inner);
		gassert(rootPosition != m_rootTransforms.end());
	}

	u32 childIter = trans->childHandle;
	// the first child of any transform should have no previous sibling, ie it should be the first sibling
	gassert(childIter == -1 || ((InternalTransform*)m_transformAllocator.GetPointerFromIndex(childIter))->prevSiblingHandle == -1);

	// make some orphans
	while (childIter != -1)
	{
		auto* child = (InternalTransform*)m_transformAllocator.GetPointerFromIndex(childIter);
		child->parentHandle = -1;
		// move this orphan up to root
		m_rootTransforms.push_back(childIter);
		childIter = child->nextSiblingHandle;
		// orphan no longer has connection to siblings
		child->nextSiblingHandle = -1;
		child->prevSiblingHandle = -1;
	}

	m_transformAllocator.Destroy(trans);
}


auto ggp::TransformHierarchy::GetFirstChild(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(h._inner < 0, "Attempt to get child of null transform");
	auto* trans = (InternalTransform*)m_transformAllocator.GetPointerFromIndex(h._inner);
	return trans->childHandle == -1 ? std::optional<Handle>{} : Handle(trans->childHandle);
}

auto ggp::TransformHierarchy::GetNextSibling(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(h._inner < 0, "Attempt to get sibling of null transform");
	auto* trans = (InternalTransform*)m_transformAllocator.GetPointerFromIndex(h._inner);
	return trans->nextSiblingHandle == -1 ? std::optional<Handle>{} : Handle(trans->nextSiblingHandle);
}

auto ggp::TransformHierarchy::GetParent(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(h._inner < 0, "Attempt to get parent of null transform");
	auto* trans = (InternalTransform*)m_transformAllocator.GetPointerFromIndex(h._inner);
	return trans->parentHandle == -1 ? std::optional<Handle>{} : Handle(trans->parentHandle);
}

auto ggp::TransformHierarchy::AddChild(Handle h) noexcept -> Handle
{
	abort_if(h._inner < 0, "Attempt to add child to null transform");
	auto* trans = (InternalTransform*)m_transformAllocator.GetPointerFromIndex(h._inner);

	auto* newChild = m_transformAllocator.Create<InternalTransform>();
	u32 newChildIndex = m_transformAllocator.GetIndexFromPointer(newChild);
	newChild->parentHandle = h._inner;

	if (trans->childHandle >= 0)
	{
		// transform already has a child, insert into linked list
		// TODO: prevSiblingHandle is not actually used by the API, maybe remove it
		auto* existingChild = (InternalTransform*)m_transformAllocator.GetPointerFromIndex(trans->childHandle);
		gassert(existingChild->prevSiblingHandle < 0);
		existingChild->prevSiblingHandle = newChildIndex;

		newChild->nextSiblingHandle = m_transformAllocator.GetIndexFromPointer(existingChild);
	}

	trans->childHandle = newChildIndex;
	return Handle(newChildIndex);
}

const DirectX::XMFLOAT4X4* ggp::TransformHierarchy::GetMatrix(Handle h) const noexcept
{
	return &GetPtr(h)->global;
}

		// getters- always okay to call these because they are entirely local to this
		// transform and have nothing to do with the heirarchy
		DirectX::XMFLOAT3A GetLocalPosition(Handle) const noexcept;
		DirectX::XMFLOAT3A GetLocalEulerAngles(Handle) const noexcept;
		DirectX::XMFLOAT3A GetLocalScale(Handle) const noexcept;

		// getters- these call into the transform hierarchy and require updates
		DirectX::XMFLOAT4X4 GetMatrix(Handle) const noexcept;
		DirectX::XMFLOAT3A GetPosition(Handle) const noexcept;
		DirectX::XMFLOAT3A GetEulerAngles(Handle) const noexcept;
		DirectX::XMFLOAT3A GetScale(Handle) const noexcept;

		// setters- cause updates in the transform hierarchy
		void SetPosition(Handle, const DirectX::XMFLOAT3A& pos) const noexcept;
		void SetEulerAngles(Handle, const DirectX::XMFLOAT3A& angles) const noexcept;
		void SetScale(Handle, const DirectX::XMFLOAT3A& scale) const noexcept;
		void SetLocalPosition(Handle, const DirectX::XMFLOAT3A& pos) const noexcept;
		void SetLocalEulerAngles(Handle, const DirectX::XMFLOAT3A& angles) const noexcept;
		void SetLocalScale(Handle, const DirectX::XMFLOAT3A& scale) const noexcept;
