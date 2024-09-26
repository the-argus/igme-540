
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
	auto* trans = GetPtr(handle);
	if (trans->parentHandle == -1) // we are in root transforms, need to remove from that so we dont get freed @ the end
	{
		auto rootPosition = std::find(m_rootTransforms.begin(), m_rootTransforms.end(), handle._inner);
		gassert(rootPosition != m_rootTransforms.end());
	}

	u32 childIter = trans->childHandle;

	// make some orphans
	while (childIter != -1)
	{
		auto* child = GetPtr(childIter);
		child->parentHandle = -1;
		// move this orphan up to root
		m_rootTransforms.push_back(childIter);
		childIter = child->nextSiblingHandle;
		// orphan no longer has connection to siblings
		child->nextSiblingHandle = -1;
	}

	m_transformAllocator.Destroy(trans);
}


auto ggp::TransformHierarchy::GetFirstChild(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(IsNull(h), "Attempt to get child of null transform");
	auto* trans = GetPtr(h);
	return trans->childHandle == -1 ? std::optional<Handle>{} : Handle(trans->childHandle);
}

auto ggp::TransformHierarchy::GetNextSibling(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(IsNull(h), "Attempt to get sibling of null transform");
	auto* trans = GetPtr(h);
	return trans->nextSiblingHandle == -1 ? std::optional<Handle>{} : Handle(trans->nextSiblingHandle);
}

auto ggp::TransformHierarchy::GetParent(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(IsNull(h), "Attempt to get parent of null transform");
	auto* trans = GetPtr(h);
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
		auto* existingChild = GetPtr(trans->childHandle);

		newChild->nextSiblingHandle = m_transformAllocator.GetIndexFromPointer(existingChild);
	}

	trans->childHandle = newChildIndex;
	return Handle(newChildIndex);
}

void ggp::TransformHierarchy::Clean(u32 transform) const noexcept
{
	m_cleaningArena.clear();

	u32 iter = transform;
	while (true)
	{
		auto* iterPtr = GetPtr(iter);
		// stop traversing upwards once we meet a transform that is not dirty
		if (!iterPtr->isDirty) [[unlikely]]
			break;
		m_cleaningArena.push_back(iter);
		if (iterPtr->parentHandle < 0) [[unlikely]]
			break;
		iter = iterPtr->parentHandle;
	}

	// weve built a stack of transforms, now multiply them downwards
	XMMATRIX mat = XMMatrixIdentity();
	for (auto i = m_cleaningArena.rbegin(); i != m_cleaningArena.rend(); ++i)
	{
		auto* ptr = GetPtr(*i);
		XMVECTOR vecRegister = XMLoadFloat3(&ptr->localPosition);
		XMMATRIX local = XMMatrixTranslationFromVector(vecRegister);
		vecRegister = XMLoadFloat3(&ptr->localRotation);
		local = XMMatrixMultiply(local, XMMatrixRotationRollPitchYawFromVector(vecRegister));
		vecRegister = XMLoadFloat3(&ptr->localScale);
		local = XMMatrixMultiply(local, XMMatrixScalingFromVector(vecRegister));
		mat = XMMatrixMultiply(mat, local);
		// store the calculated global matrix for this object
		XMStoreFloat4x4(&ptr->matrix, mat);
		ptr->isDirty = false;
	}
	gassert(!GetPtr(transform)->isDirty);
}

void ggp::TransformHierarchy::MarkDirty(u32 transform) const noexcept
{
	m_dirtyStack.clear();
	DirtyStackFrame frame{ .transform = transform };
	while (true)
	{
		// using this vector instead of the call stack like we would in a recursive solution
		InternalTransform* current = GetPtr(frame.transform);
		// depth first
		if (!frame.triedChild && current->childHandle >= 0)
		{
			frame.triedChild = true;
			m_dirtyStack.push_back(frame);
			frame = DirtyStackFrame{ .transform = current->childHandle };
			continue; // recurse
		}
		else if (!frame.triedSibling && current->nextSiblingHandle >= 0)
		{
			frame.triedSibling = true;
			m_dirtyStack.push_back(frame);
			frame = DirtyStackFrame{ .transform = current->nextSiblingHandle };
			continue; // recurse
		}

		// okay, no children and no next siblings. mark dirty
		current->isDirty = true;
		if (m_dirtyStack.empty())
			break;
		frame = m_dirtyStack.back();
		m_dirtyStack.pop_back();
	}
	m_dirtyStack.clear();
}

const DirectX::XMFLOAT4X4* ggp::TransformHierarchy::GetMatrix(Handle h) const noexcept
{
	auto* trans = GetPtr(h);
	if (trans->isDirty) {
		Clean(h._inner);
	}
	gassert(!trans->isDirty);
	return &trans->matrix;
}
