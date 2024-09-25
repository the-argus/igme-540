
#include "TransformHierarchy.h"
#include "memutils.h"


// a transform row consists of three allocators which all allocate and release at the same time
explicit ggp::TransformHierarchy::TransformRow::TransformRow(size_t size) noexcept
	: matrices(BlockAllocator::Options{
	.maxBytes = 100_MB,
		.initialBytes = 1_MB,
		.blockSize = sizeof(InternalTransformMatrices) * size,
		.minimumAlignmentExponent = alignment_exponent(alignof(InternalTransformMatrices)),
}),
transforms(BlockAllocator::Options{
	.maxBytes = 100_MB,
		.initialBytes = 1_MB,
		.blockSize = sizeof(InternalTransform) * size,
		.minimumAlignmentExponent = alignment_exponent(alignof(InternalTransform)),
}),
treeDatas(BlockAllocator::Options{
	.maxBytes = 100_MB,
		.initialBytes = 1_MB,
		.blockSize = sizeof(InternalTreeData) * size,
		.minimumAlignmentExponent = alignment_exponent(alignof(InternalTreeData)),
})
{
}

// transform heirarchy has a pool for big transforms, and two sets of allocators for transforms
// with no siblings and transforms with 1-3 siblings.
ggp::TransformHierarchy::TransformHierarchy() noexcept :
	m_bigTransformPool(BlockAllocator::Options{
	.maxBytes = 100_MB,
		.initialBytes = 0_MB,
		.blockSize = sizeof(BigTransformData),
		.minimumAlignmentExponent = alignment_exponent(alignof(BigTransformData)),
}),
m_rows{ TransformRow(siblingAmounts[0]), TransformRow(siblingAmounts[1]) }
{
}

u32 ggp::TransformHierarchy::InsertInRow(RowType rowType) noexcept
{
	switch (rowType)
	{
	case RowType::Large: {
		auto* bigData = m_bigTransformPool.Create<BigTransformData>();
		return m_bigTransformPool.GetIndexFromPointer(bigData);
	}
	case RowType::Small:
	case RowType::Medium: {
		auto& row = m_rows[u8(rowType)];
		std::span<u8> matricesMem = row.matrices.Alloc();
		std::span<u8> transformMem = row.transforms.Alloc();
		std::span<u8> treeDataMem = row.treeDatas.Alloc();

		u32 handle = row.matrices.GetIndexFromPointer(matricesMem.data());
		gassert(row.transforms.GetIndexFromPointer(transformMem.data()) == handle);
		gassert(row.treeDatas.GetIndexFromPointer(treeDataMem.data()) == handle);
		return handle;
	}
	}
	gabort();
}

auto ggp::TransformHierarchy::InsertTransform() noexcept -> TransformHandle
{
	return TransformHandle(InsertInRow(RowType::Small), u8(RowType::Small));
}

// destroy a given transform, subtracting from the number of children its parent had.
// if the destroyed transform had children, it becomes one of the root transforms.
void ggp::TransformHierarchy::DestroyTransform(TransformHandle handle) noexcept
{
	auto rowType = static_cast<RowType>(handle.extra);

	switch (rowType)
	{
	case RowType::Large:
		auto* bigData = (BigTransformData*)m_bigTransformPool.GetPointerFromIndex(handle.id);
	case RowType::Small:
	case RowType::Medium:
		TransformRow& row = m_rows[u8(rowType)];
		auto* matrices = (InternalTransformMatrices*)row.matrices.GetPointerFromIndex(handle.id);
		auto* transform = (InternalTransform*)row.transforms.GetPointerFromIndex(handle.id);
		auto* treeData = (InternalTreeData*)row.treeDatas.GetPointerFromIndex(handle.id);

		row.matrices.Destroy(matrices);
		row.transforms.Destroy(transform);
		row.treeDatas.Destroy(treeData);
	}
}
