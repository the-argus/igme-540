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

	private:
		enum class TransformStorageLocationFlags : u8
		{
			NoChildren = 0b0000,
			FewChildren = 0b0001,
			MediumChildren = 0b0010,
			ManyChildren = 0b0100,
		};

		struct InternalTransform
		{
			DirectX::XMFLOAT3 localPosition;
			DirectX::XMFLOAT3 localRotation;
			DirectX::XMFLOAT3 localScale;
		};

		struct InternalTreeData
		{
			u32 matrixHandle;
			u32 parentHandle;
			u32 childrenHandle;
			bool isDirty;
			TransformStorageLocationFlags parentLocation;
			TransformStorageLocationFlags childLocation;
		};

		struct InternalTransformMatrices
		{
			DirectX::XMFLOAT4X4A local;
			DirectX::XMFLOAT4X4A global;
		};

		// a row of transform data is a set of three big arrays (block allocators).
		// each element of these arrays are also arrays. each sub array is the same
		// size for each block allocator in the same row.
		struct TransformRow
		{
			explicit TransformRow(size_t size) noexcept;
			BlockAllocator treeDatas; // InternalTreeData * size
			BlockAllocator transforms; // InternalTransform * size
			BlockAllocator matrices; // InternalTransformMatrices * size
		};

		// if a transform has a ton of children, we don't bother with block allocator business, just put it on the head w/ vectors
		struct BigTransformData
		{
			std::vector<InternalTreeData> treeDatas;
			std::vector<InternalTransform> transforms;
			std::vector<InternalTransformMatrices> matrices;
		};

		static_assert(sizeof InternalTransform == 36);
		static_assert(sizeof InternalTreeData == 16);

	private:
		enum class RowType: u8
		{
			Small = 0,
			Medium = 1,
			Large,
		};

		u32 InsertInRow(RowType idx) noexcept;

		// transforms with no parent
		std::vector<u32> m_rootTransformHandles;
		std::vector<TransformStorageLocationFlags> m_rootTransformLocations;

		// pool of big transforms, stores BigTransformData
		BlockAllocator m_bigTransformPool;

		// store pools of groups of 1 and groups of 4 transforms
		static constexpr u64 siblingAmounts[] = {1, 4};
		TransformRow m_rows[2];
	};
}
