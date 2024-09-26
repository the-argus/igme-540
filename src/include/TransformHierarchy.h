#pragma once

#include <DirectXMath.h>

#include <vector>
#include <optional>

#include "BlockAllocator.h"

#define TH_VECTORCALL __vectorcall

namespace ggp
{
	class TransformHierarchy
	{
	public:
		/// <summary>
		/// A small, opaque object like a ticket which can be given to a transform hierarchy to retrieve information
		/// about the associated transform.
		/// </summary>
		struct Handle
		{
			friend class TransformHierarchy;
		private:
			i32 _inner;
			inline constexpr Handle(u32 idx) noexcept  : _inner(idx) {}
		};

		TransformHierarchy() noexcept;

		/// <summary>
		/// Initialize a new transform in the system, with no parent.
		/// </summary>
		/// <returns>A handle pointing to the newly allocated transform.</returns>
		Handle InsertTransform() noexcept;

		// tree traversal
		std::optional<Handle> GetFirstChild(Handle) const noexcept;
		std::optional<Handle> GetNextSibling(Handle) const noexcept;
		std::optional<Handle> GetParent(Handle) const noexcept;

		// tree modification
		Handle AddChild(Handle) noexcept;
		void Destroy(Handle) noexcept;

		// getters- always okay to call these because they are entirely local to this
		// transform and have nothing to do with the heirarchy
		inline DirectX::XMVECTOR LoadLocalPosition(Handle) const noexcept;
		inline DirectX::XMVECTOR LoadLocalEulerAngles(Handle) const noexcept;
		inline DirectX::XMVECTOR LoadLocalScale(Handle) const noexcept;

		// getters- these call into the transform hierarchy and require updates

		/// <summary>
		/// Get a pointer to the calculated world matrix for this transform.
		/// This does not load into a XMMATRIX because it is intended for
		/// memcpy into gpu mapped buffer
		/// </summary>
		/// <param name="">The handle pointing to the transform to load.</param>
		/// <returns>A pointer to the calculated matrix. Using this pointer after other operations have been performed on the hierarchy is undefined.</returns>
		const DirectX::XMFLOAT4X4* GetMatrix(Handle) const noexcept;

		inline DirectX::XMVECTOR LoadPosition(Handle) const noexcept;
		inline DirectX::XMVECTOR LoadEulerAngles(Handle) const noexcept;
		inline DirectX::XMVECTOR LoadScale(Handle) const noexcept;

		// setters- cause updates in the transform hierarchy
		inline void TH_VECTORCALL StorePosition(Handle, DirectX::XMVECTOR pos) noexcept;
		inline void TH_VECTORCALL StoreEulerAngles(Handle, DirectX::XMVECTOR angles) noexcept;
		inline void TH_VECTORCALL StoreScale(Handle, DirectX::XMVECTOR scale) noexcept;
		inline void TH_VECTORCALL StoreLocalPosition(Handle, DirectX::XMVECTOR pos) noexcept;
		inline void TH_VECTORCALL StoreLocalEulerAngles(Handle, DirectX::XMVECTOR angles) noexcept;
		inline void TH_VECTORCALL StoreLocalScale(Handle, DirectX::XMVECTOR scale) noexcept;

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

		inline constexpr InternalTransform* GetPtr(Handle h) const noexcept
		{
			return (InternalTransform*)m_transformAllocator.GetPointerFromIndex(h._inner);
		}

		inline constexpr bool IsNull(Handle h) const noexcept { return h._inner < 0; }

		std::vector<u32> m_rootTransforms;
		BlockAllocator m_transformAllocator;
	};

	// inline simd function definitions -----------------------------------------------------

	inline DirectX::XMVECTOR TransformHierarchy::LoadLocalPosition(Handle h) const noexcept
	{
		// using assert so that this goes away in release mode, to avoid potentially calling functins and messing with registers
		// this does not match up with non inlined functions which always do abort_if to null check
		gassert(!IsNull(h), "attempt to get local position of null transform");
		return DirectX::XMLoadFloat3(&GetPtr(h)->localPosition);
	}

	inline DirectX::XMVECTOR TransformHierarchy::LoadLocalEulerAngles(Handle h) const noexcept
	{
		gassert(!IsNull(h), "attempt to get local euler angles of null transform");
		return DirectX::XMLoadFloat3(&GetPtr(h)->localRotation);
	}

	inline DirectX::XMVECTOR TransformHierarchy::LoadLocalScale(Handle h) const noexcept
	{
		gassert(!IsNull(h), "attempt to get local scale of null transform");
		return DirectX::XMLoadFloat3(&GetPtr(h)->localScale);
	}

	inline void TH_VECTORCALL TransformHierarchy::StoreLocalPosition(Handle h, DirectX::XMVECTOR pos) noexcept
	{
		gassert(!IsNull(h), "attempt to change the local position of null transform");
		DirectX::XMStoreFloat3(&GetPtr(h)->localPosition, pos);
	}

	inline void TH_VECTORCALL TransformHierarchy::StoreLocalEulerAngles(Handle h, DirectX::XMVECTOR angles) noexcept
	{
		gassert(!IsNull(h), "attempt to change the local euler angles of null transform");
		DirectX::XMStoreFloat3(&GetPtr(h)->localRotation, angles);
	}

	inline void TH_VECTORCALL TransformHierarchy::StoreLocalScale(Handle h, DirectX::XMVECTOR scale) noexcept
	{
		gassert(!IsNull(h), "attempt to change the local euler angles of null transform");
		DirectX::XMStoreFloat3(&GetPtr(h)->localScale, scale);
	}
}
