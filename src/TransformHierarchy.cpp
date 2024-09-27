
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

	i32 childIter = trans->childHandle;

	// disconnect from sibling (iterate through singly linked list to find previous sibling)
	if (!IsNull(trans->parentHandle))
	{
		i32 prev = -1;
		auto* parent = GetPtr(trans->parentHandle);
		i32 siblingIter = parent->childHandle;
		gassert(!IsNull(siblingIter));
		// handle case that we are the first child
		if (siblingIter == handle._inner)
		{
			parent->childHandle = trans->nextSiblingHandle;
		} 
		else
		{
			// if we are not first child, loop until we get to somebody whose next sibling is us
			while (true)
			{
				auto* current = GetPtr(siblingIter);
				if (IsNull(current->nextSiblingHandle))
					break;
				if (current->nextSiblingHandle == handle._inner)
				{
					// we are the next sibling, the current sibling is previous
					current->nextSiblingHandle = trans->nextSiblingHandle;
					break;
				}
				siblingIter = current->nextSiblingHandle;
			}
		}
	}

	// just to be sure nothing else tries to traverse to children
	trans->childHandle = -1;

	// make some orphans
	// apply parent transform to local transform of children
	while (!IsNull(childIter))
	{
		auto* child = GetPtr(childIter);
		XMVECTOR globalPosition;
		XMVECTOR globalRotation;
		XMVECTOR globalScale;
		LoadMatrixDecomposed(childIter, &globalPosition, &globalRotation, &globalScale);
		child->parentHandle = -1;
		childIter = child->nextSiblingHandle;
		// orphan no longer has connection to siblings
		child->nextSiblingHandle = -1;
		
		// convert to eulers, requires going in and out of simd registers :(
		XMFLOAT4 quat;
		XMStoreFloat4(&quat, globalRotation);
		XMFLOAT3 eulers = QuatToEuler(quat);
		globalRotation = XMLoadFloat3(&eulers);

		// restore global position
		StorePosition(childIter, globalPosition);
		StoreEulerAngles(childIter, globalRotation);
		StoreScale(childIter, globalScale);
	}

	m_transformAllocator.Destroy(trans);
}


auto ggp::TransformHierarchy::GetFirstChild(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(IsNull(h), "Attempt to get child of null transform");
	auto* trans = GetPtr(h);
	return IsNull(trans->childHandle) ? std::optional<Handle>{} : Handle(trans->childHandle);
}

auto ggp::TransformHierarchy::GetNextSibling(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(IsNull(h), "Attempt to get sibling of null transform");
	auto* trans = GetPtr(h);
	return IsNull(trans->nextSiblingHandle) ? std::optional<Handle>{} : Handle(trans->nextSiblingHandle);
}

auto ggp::TransformHierarchy::GetParent(Handle h) const noexcept -> std::optional<Handle>
{
	abort_if(IsNull(h), "Attempt to get parent of null transform");
	auto* trans = GetPtr(h);
	return IsNull(trans->parentHandle) ? std::optional<Handle>{} : Handle(trans->parentHandle);
}

auto ggp::TransformHierarchy::AddChild(Handle h) noexcept -> Handle
{
	abort_if(IsNull(h), "Attempt to add child to null transform");
	auto* trans = GetPtr(h);

	auto* newChild = m_transformAllocator.Create<InternalTransform>();
	u32 newChildIndex = m_transformAllocator.GetIndexFromPointer(newChild);
	newChild->parentHandle = h._inner;

	if (!IsNull(trans->childHandle))
	{
		// transform already has a child, insert into linked list
		auto* existingChild = GetPtr(trans->childHandle);

		newChild->nextSiblingHandle = m_transformAllocator.GetIndexFromPointer(existingChild);
		newChild->isDirty = true; // needs to be calculated from parent, unlike InsertTransform
	}

	trans->childHandle = newChildIndex;
	return Handle(newChildIndex);
}

void ggp::TransformHierarchy::Clean(u32 transform) const noexcept
{
	m_cleaningArena.clear();

	i32 iter = transform;
	while (true)
	{
		auto* iterPtr = GetPtr(iter);
		m_cleaningArena.push_back(iter);
		// stop traversing upwards once we meet a transform that is not dirty
		if (!iterPtr->isDirty) [[unlikely]]
			break;
		if (IsNull(iterPtr->parentHandle)) [[unlikely]]
			break;
		iter = iterPtr->parentHandle;
	}

	// weve built a stack of transforms, now multiply them downwards
	XMMATRIX mat = XMMatrixIdentity();
	for (auto i = m_cleaningArena.rbegin(); i != m_cleaningArena.rend(); ++i)
	{
		auto* ptr = GetPtr(*i);
		if (!ptr->isDirty)
		{
			// not dirty, this is the first item and it should start the transform chain
			mat = XMLoadFloat4x4(&ptr->worldMatrix);
			continue;
		}
		XMVECTOR vecRegister = XMLoadFloat3(&ptr->localScale); // S
		XMMATRIX local = XMMatrixScalingFromVector(vecRegister);
		vecRegister = XMLoadFloat3(&ptr->localRotation); // R
		local = XMMatrixMultiply(local, XMMatrixRotationRollPitchYawFromVector(vecRegister));
		vecRegister = XMLoadFloat3(&ptr->localPosition); // T
		local = XMMatrixMultiply(local, XMMatrixTranslationFromVector(vecRegister));
		mat = XMMatrixMultiply(local, mat);
		// store the calculated global matrix for this object
		XMStoreFloat4x4(&ptr->worldMatrix, mat);
		XMStoreFloat4x4(&ptr->worldInverseTransposeMatrix, XMMatrixInverse(0, XMMatrixTranspose(mat)));
		ptr->isDirty = false;
	}
	gassert(!GetPtr(transform)->isDirty);
}

void ggp::TransformHierarchy::MarkDirty(u32 transform) const noexcept
{
	m_dirtyStack.clear();
	DirtyStackFrame frame{ .transform = i32(transform) };
	while (true)
	{
		// using this vector instead of the call stack like we would in a recursive solution
		InternalTransform* current = GetPtr(frame.transform);
		// depth first
		if (!frame.triedChild && !IsNull(current->childHandle))
		{
			frame.triedChild = true;
			m_dirtyStack.push_back(frame);
			frame = DirtyStackFrame{ .transform = current->childHandle };
			continue; // recurse
		}
		else if (!frame.triedSibling && !IsNull(current->nextSiblingHandle))
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

const DirectX::XMFLOAT4X4* ggp::TransformHierarchy::GetWorldMatrixPtr(Handle h) const noexcept
{
	auto* trans = GetPtr(h);
	if (trans->isDirty) {
		Clean(h._inner);
	}
	gassert(!trans->isDirty);
	return &trans->worldMatrix;
}

const DirectX::XMFLOAT4X4* ggp::TransformHierarchy::GetWorldInverseTransposeMatrixPtr(Handle h) const noexcept
{
	auto* trans = GetPtr(h);
	if (trans->isDirty) {
		Clean(h._inner);
	}
	gassert(!trans->isDirty);
	return &trans->worldInverseTransposeMatrix;
}

ggp::TransformHierarchy::InternalTransform::InternalTransform() noexcept
{
	// the NSDMIs handle most stuff, except the matrices
	XMStoreFloat4x4(&worldMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixIdentity());
}

DirectX::XMFLOAT3 ggp::TransformHierarchy::GetLocalPosition(Handle h) const
{
	return GetPtr(h)->localPosition;
}

DirectX::XMFLOAT3  ggp::TransformHierarchy::GetLocalRotation(Handle h) const
{
	return GetPtr(h)->localRotation;
}

DirectX::XMFLOAT3  ggp::TransformHierarchy::GetLocalScale(Handle h) const
{
	return GetPtr(h)->localScale;
}

void  ggp::TransformHierarchy::SetLocalPosition(Handle h, DirectX::XMFLOAT3 position)
{
	GetPtr(h)->localPosition = position;
	MarkDirty(h._inner);
}

void  ggp::TransformHierarchy::SetLocalRotation(Handle h, DirectX::XMFLOAT3 rotation)
{
	GetPtr(h)->localRotation = rotation;
	MarkDirty(h._inner);
}

void  ggp::TransformHierarchy::SetLocalScale(Handle h, DirectX::XMFLOAT3 scale)
{
	GetPtr(h)->localScale = scale;
	MarkDirty(h._inner);
}
