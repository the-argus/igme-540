#pragma once

#include <DirectXMath.h>

#include <vector>

#include "BlockAllocator.h"
#include "TransformHandle.h"

namespace ggp
{
	class TransformHierarchy
	{
	public:
		TransformHierarchy() noexcept;

		// initialize a new transform in the system and return a handle to it
		TransformHandle InsertTransform() noexcept;

		// destroy a given transform, subtracting from the number of children its parent had.
		// if the destroyed transform had children, it becomes one of the root transforms.
		void DestroyTransform(TransformHandle handle) noexcept;

		friend class TransformHandle;

	private:
		struct InternalTransform
		{
			DirectX::XMFLOAT3A localPosition = {};
			DirectX::XMFLOAT3A localRotation = {};
			DirectX::XMFLOAT3A localScale = {1, 1, 1};
			DirectX::XMFLOAT4X4A local = {};
			DirectX::XMFLOAT4X4A global = {};
			i32 parentHandle = -1;
			i32 nextSiblingHandle = -1;
			i32 prevSiblingHandle = -1;
			i32 childHandle = -1;
			bool isDirty = false;
		};

		std::vector<u32> m_rootTransforms;
		BlockAllocator m_transformAllocator;
	};
}
