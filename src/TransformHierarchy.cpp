
#include "TransformHierarchy.h"
#include "memutils.h"

ggp::TransformHierarchy::TransformHierarchy() noexcept :m_transformAllocator(BlockAllocator::Options{
	.maxBytes = 10_MB,
		.initialBytes = 1_MB,
		.blockSize = sizeof(InternalTransform),
		.minimumAlignmentExponent = alignment_exponent(alignof(InternalTransform))
})
{
}

auto ggp::TransformHierarchy::InsertTransform() noexcept -> TransformHandle
{
	auto* ptr = m_transformAllocator.Create<InternalTransform>();
	return TransformHandle(m_transformAllocator.GetIndexFromPointer(ptr), this);
}

void ggp::TransformHierarchy::DestroyTransform(TransformHandle handle) noexcept
{
	auto* trans = (InternalTransform*)m_transformAllocator.GetPointerFromIndex(handle.id);
	if (trans->parentHandle == -1) // we are in root transforms, need to remove from that so we dont get freed @ the end
	{
		auto rootPosition = std::find(m_rootTransforms.begin(), m_rootTransforms.end(), handle.id);
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
