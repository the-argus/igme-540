
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
		gassert(parent->childCount > 0, "transform with children has child count of 0");
		parent->childCount--;
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

u32 ggp::TransformHierarchy::GetChildCount(Handle h) const noexcept
{
	abort_if(IsNull(h), "Attempt to get child count of null transform");
	return GetPtr(h)->childCount;
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
	trans->childCount++;
	return Handle(newChildIndex);
}

void ggp::TransformHierarchy::Clean(u32 transform) const noexcept
{
	gassert(m_cleaningArena.empty(), "recursive call to Clean()?");

	u32 iter = transform;
	while (!IsNull(iter))
	{
		m_cleaningArena.push_back(iter);

		InternalTransform* iterPtr = GetPtr(iter);
		//if (!iterPtr->isDirty) [[unlikely]]
		//	break;
		iter = u32(iterPtr->parentHandle);
	}

	// weve built a stack of transforms, now multiply them downwards (like down a family tree)
	XMMATRIX mat = XMMatrixIdentity();
	//for (const auto& i : m_cleaningArena | std::views::reverse)
	for (auto i = m_cleaningArena.rbegin(); i != m_cleaningArena.rend(); ++i)
	{
		InternalTransform* const ptr = GetPtr(*i);

		const XMVECTOR localPosition = XMLoadFloat3(&ptr->localPosition);
		const XMVECTOR localRotation = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&ptr->localRotation));
		const XMVECTOR localScale = XMLoadFloat3(&ptr->localScale);
		gassert(!XMVector3IsNaN(localPosition));
		gassert(!XMVector3IsNaN(localRotation));
		gassert(!XMVector3IsNaN(localScale));
		
		const XMMATRIX localTransform = XMMatrixAffineTransformation(
			localScale,
			g_XMZero.v,
			localRotation,
			localPosition);
		gassert(!XMMatrixIsNaN(localTransform));
	
		mat = XMMatrixMultiply(localTransform, mat);
		gassert(!XMMatrixIsNaN(mat));

		// store the calculated global matrix for this object
		XMStoreFloat4x4(&ptr->worldMatrix, mat);
		XMStoreFloat4x4(&ptr->worldInverseTransposeMatrix, XMMatrixInverse(0, XMMatrixTranspose(mat)));
		ptr->isDirty = false;
	}
	gassert(!GetPtr(transform)->isDirty);
	m_cleaningArena.clear();
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

DirectX::XMFLOAT3  ggp::TransformHierarchy::GetLocalEulerAngles(Handle h) const
{
	return GetPtr(h)->localRotation;
}

DirectX::XMFLOAT3  ggp::TransformHierarchy::GetLocalScale(Handle h) const
{
	return GetPtr(h)->localScale;
}

void  ggp::TransformHierarchy::SetLocalPosition(Handle h, DirectX::XMFLOAT3 position)
{
	using namespace DirectX;
	gassert(!XMVector3IsNaN(XMLoadFloat3(&position)));
	gassert(!XMVector3IsInfinite(XMLoadFloat3(&position)));
	GetPtr(h)->localPosition = position;
	MarkDirty(h._inner);
}

void  ggp::TransformHierarchy::SetLocalEulerAngles(Handle h, DirectX::XMFLOAT3 rotation)
{
	using namespace DirectX;
	gassert(!XMVector3IsNaN(XMLoadFloat3(&rotation)));
	gassert(!XMVector3IsInfinite(XMLoadFloat3(&rotation)));
	GetPtr(h)->localRotation = rotation;
	MarkDirty(h._inner);
}

void  ggp::TransformHierarchy::SetLocalScale(Handle h, DirectX::XMFLOAT3 scale)
{
	using namespace DirectX;
	gassert(!XMVector3IsNaN(XMLoadFloat3(&scale)));
	gassert(!XMVector3IsInfinite(XMLoadFloat3(&scale)));
	GetPtr(h)->localScale = scale;
	MarkDirty(h._inner);
}

XMFLOAT3 ggp::TransformHierarchy::GetPosition(Handle h) const
{
	XMFLOAT3 out;
	XMStoreFloat3(&out, LoadPosition(h));
	return out;
}

XMFLOAT3 ggp::TransformHierarchy::GetEulerAngles(Handle h) const
{
	XMFLOAT3 out;
	XMStoreFloat3(&out, LoadEulerAngles(h));
	return out;
}

XMFLOAT3 ggp::TransformHierarchy::GetScale(Handle h) const
{
	XMFLOAT3 out;
	XMStoreFloat3(&out, LoadScale(h));
	return out;
}

void ggp::TransformHierarchy::SetPosition(Handle h, XMFLOAT3 position)
{
	StorePosition(h, XMLoadFloat3(&position));
}

void ggp::TransformHierarchy::SetEulerAngles(Handle h, XMFLOAT3 rotation)
{
	StoreEulerAngles(h, XMLoadFloat3(&rotation));
}

void ggp::TransformHierarchy::SetScale(Handle h, XMFLOAT3 scale)
{
	StoreScale(h, XMLoadFloat3(&scale));
}
