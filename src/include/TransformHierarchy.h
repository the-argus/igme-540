#pragma once

#include <DirectXMath.h>

#include <vector>
#include <optional>

#include "BlockAllocator.h"
#include "ggp_math.h"

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
			inline constexpr Handle(u32 idx) noexcept : _inner(idx) {}
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
		u32 GetChildCount(Handle) const noexcept;

		// tree modification
		Handle AddChild(Handle) noexcept;
		void Destroy(Handle) noexcept;

		const DirectX::XMFLOAT4X4* GetWorldMatrixPtr(Handle) const noexcept;
		const DirectX::XMFLOAT4X4* GetWorldInverseTransposeMatrixPtr(Handle) const noexcept;

		inline void LoadMatrixDecomposed(
			Handle,
			DirectX::XMVECTOR* outPos,
			DirectX::XMVECTOR* outQuat,
			DirectX::XMVECTOR* outScale) const noexcept;

		DirectX::XMFLOAT3 GetPosition(Handle) const;
		inline DirectX::XMVECTOR LoadPosition(Handle) const noexcept;
		DirectX::XMFLOAT3 GetEulerAngles(Handle) const;
		inline DirectX::XMVECTOR LoadEulerAngles(Handle) const noexcept;
		DirectX::XMFLOAT3 GetScale(Handle) const;
		inline DirectX::XMVECTOR LoadScale(Handle) const noexcept;
		DirectX::XMFLOAT3 GetLocalPosition(Handle) const;
		inline DirectX::XMVECTOR LoadLocalPosition(Handle) const noexcept;
		DirectX::XMFLOAT3 GetLocalEulerAngles(Handle) const;
		inline DirectX::XMVECTOR LoadLocalEulerAngles(Handle) const noexcept;
		DirectX::XMFLOAT3 GetLocalScale(Handle) const;
		inline DirectX::XMVECTOR LoadLocalScale(Handle) const noexcept;

		void SetPosition(Handle, DirectX::XMFLOAT3 position);
		inline void TH_VECTORCALL StorePosition(Handle, DirectX::FXMVECTOR pos) noexcept;
		void SetEulerAngles(Handle, DirectX::XMFLOAT3 rotation);
		inline void TH_VECTORCALL StoreEulerAngles(Handle, DirectX::FXMVECTOR rotation) noexcept;
		void SetScale(Handle, DirectX::XMFLOAT3 scale);
		inline void TH_VECTORCALL StoreScale(Handle, DirectX::FXMVECTOR scale) noexcept;
		void SetLocalPosition(Handle, DirectX::XMFLOAT3 position);
		inline void TH_VECTORCALL StoreLocalPosition(Handle, DirectX::FXMVECTOR pos) noexcept;
		void SetLocalEulerAngles(Handle, DirectX::XMFLOAT3 rotation);
		inline void TH_VECTORCALL StoreLocalEulerAngles(Handle, DirectX::FXMVECTOR angles) noexcept;
		void SetLocalScale(Handle, DirectX::XMFLOAT3 scale);
		inline void TH_VECTORCALL StoreLocalScale(Handle, DirectX::FXMVECTOR scale) noexcept;


	private:
		struct InternalTransform
		{
			DirectX::XMFLOAT3 localPosition = {};
			DirectX::XMFLOAT3 localRotation = {};
			DirectX::XMFLOAT3 localScale = { 1, 1, 1 };
			DirectX::XMFLOAT4X4A worldMatrix;
			DirectX::XMFLOAT4X4A worldInverseTransposeMatrix;
			i32 parentHandle = -1;
			i32 nextSiblingHandle = -1;
			i32 childHandle = -1;
			u32 childCount = 0;
			bool isDirty = false;

			InternalTransform() noexcept;
		};

		inline constexpr InternalTransform* GetPtr(Handle h) const noexcept
		{
			return (InternalTransform*)m_transformAllocator.GetPointerFromIndex(h._inner);
		}

		inline constexpr bool IsNull(Handle h) const noexcept { return h._inner < 0; }

		// take a dirty transform and move up until finding the earliest clean ancestor, then propagate all changes down, cleaning
		// any passed children on the way. stops when it cleans the target transform
		void Clean(u32 transform) const noexcept;

		// recursively mark a transform and all children as dirty. in theory this is super
		// innefficient bc linked list + bools in structs, every node read is a cache line evicted.
		// TODO: benchmark stuff, probably
		void MarkDirty(u32 transform) const noexcept;

		struct DirtyStackFrame
		{
			i32 transform = -1;
			u8 triedChild = false;
			u8 triedSibling = false;
		};

		// when calling Clean, we push handles to this during it and then clear it when Clean is done
		mutable std::vector<u32> m_cleaningArena;
		// for recursing into the tree without using call stack recursion, we still need a stack of some kind
		mutable std::vector<DirtyStackFrame> m_dirtyStack;
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

	inline void TH_VECTORCALL TransformHierarchy::StoreLocalPosition(Handle h, DirectX::FXMVECTOR pos) noexcept
	{
		using namespace DirectX;
		gassert(!IsNull(h), "attempt to change the local position of null transform");
		gassert(!XMVector3IsNaN(pos));
		XMStoreFloat3(&GetPtr(h)->localPosition, pos);
		MarkDirty(h._inner);
	}

	inline void TH_VECTORCALL TransformHierarchy::StoreLocalEulerAngles(Handle h, DirectX::FXMVECTOR angles) noexcept
	{
		using namespace DirectX;
		gassert(!IsNull(h), "attempt to change the local euler angles of null transform");
		gassert(!XMVector3IsNaN(angles));
		XMStoreFloat3(&GetPtr(h)->localRotation, angles);
		MarkDirty(h._inner);
	}

	inline void TH_VECTORCALL TransformHierarchy::StoreLocalScale(Handle h, DirectX::FXMVECTOR scale) noexcept
	{
		using namespace DirectX;
		gassert(!IsNull(h), "attempt to change the local euler angles of null transform");
		gassert(!XMVector3IsNaN(scale));
		XMStoreFloat3(&GetPtr(h)->localScale, scale);
		MarkDirty(h._inner);
	}

	inline void TransformHierarchy::LoadMatrixDecomposed(
		Handle h,
		DirectX::XMVECTOR* outPos,
		DirectX::XMVECTOR* outQuat,
		DirectX::XMVECTOR* outScale) const noexcept
	{
		gassert(!IsNull(h), "attempt to load the matrix of null transform");
		auto* trans = GetPtr(h);

		if (trans->isDirty)
			Clean(h._inner);

		DirectX::XMMATRIX mat = XMLoadFloat4x4(&trans->worldMatrix);
		const bool success = XMMatrixDecompose(outScale, outQuat, outPos, mat);
		// this should never fail, hopefully the transform wrapper prevents matrix from ever becoming invalid or something
		assert(success);
	}

	inline DirectX::XMVECTOR TransformHierarchy::LoadPosition(Handle h) const noexcept
	{
		DirectX::XMVECTOR pos;
		DirectX::XMVECTOR quat;
		DirectX::XMVECTOR scale;
		LoadMatrixDecomposed(h, &pos, &quat, &scale);
		return pos;
	}

	inline DirectX::XMVECTOR TransformHierarchy::LoadEulerAngles(Handle h) const noexcept
	{
		gassert(!IsNull(h), "Attempt to load euler angles from null transform");
		auto* trans = GetPtr(h);
		if (trans->isDirty)
			Clean(h._inner);
		return ExtractEulersFromMatrix(&trans->worldMatrix);
	}

	inline DirectX::XMVECTOR TransformHierarchy::LoadScale(Handle h) const noexcept
	{
		DirectX::XMVECTOR pos;
		DirectX::XMVECTOR quat;
		DirectX::XMVECTOR scale;
		LoadMatrixDecomposed(h, &pos, &quat, &scale);
		return scale;
	}

	inline void TH_VECTORCALL TransformHierarchy::StorePosition(Handle h, DirectX::FXMVECTOR pos) noexcept
	{
		using namespace DirectX;

		auto* trans = GetPtr(h);
		auto* worldStorage = GetWorldMatrixPtr(h);
		XMMATRIX world = XMLoadFloat4x4(worldStorage);

		// get our contribution to world space
		XMMATRIX local = XMMatrixMultiply(
			XMMatrixMultiply(
				XMMatrixScalingFromVector(XMLoadFloat3(&trans->localScale)),
				XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&trans->localRotation))),
			XMMatrixTranslationFromVector(XMLoadFloat3(&trans->localPosition)));
		// revert our contribution from world, getting the space that our position is in
		world = XMMatrixMultiply(XMMatrixInverse(nullptr, local), world);

		// translate the global position into this parent space
		XMVECTOR localPos = XMVector3Transform(pos, world);
		StoreLocalPosition(h, localPos);
	}

	inline void TH_VECTORCALL TransformHierarchy::StoreEulerAngles(Handle h, DirectX::FXMVECTOR angles) noexcept
	{
		using namespace DirectX;
		XMVECTOR localQuat; // innaccurate name, at first
		XMVECTOR globalQuat;
		XMVECTOR targetQuat; // innaccurate name, at first
		LoadMatrixDecomposed(h, &localQuat, &globalQuat, &targetQuat);

		targetQuat = XMQuaternionRotationRollPitchYawFromVector(angles);
		localQuat = XMQuaternionRotationRollPitchYawFromVector(LoadLocalEulerAngles(h));
		// "subtract" the contribution of the local angles- effectively the parent's global rotation
		globalQuat = XMQuaternionMultiply(globalQuat, XMQuaternionInverse(localQuat));
		// target is now delta-to-target
		targetQuat = XMQuaternionMultiply(targetQuat, XMQuaternionInverse(globalQuat));

		XMFLOAT4 quaternionForm;
		XMStoreFloat4(&quaternionForm, targetQuat);
		SetLocalEulerAngles(h, QuatToEuler(quaternionForm));
	}

	inline void TH_VECTORCALL TransformHierarchy::StoreScale(Handle h, DirectX::FXMVECTOR scale) noexcept
	{
		using namespace DirectX;
		// modifying global scale should modify local scale, but just do it in global space.
		XMVECTOR delta; // innaccurate name
		XMVECTOR localScale; // this name is innaccurate at first, actually quat
		XMVECTOR globalScale; // what we care about
		LoadMatrixDecomposed(h, &delta, &localScale, &globalScale);
		localScale = LoadLocalScale(h); // okay now its accurate
		delta = XMVectorSubtract(scale, globalScale); // difference between the target scale and our scale
		StoreLocalScale(h, XMVectorAdd(localScale, delta)); // just add the needed difference to local
	}
}
