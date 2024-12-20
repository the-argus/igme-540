#pragma once
#include <DirectXMath.h>
#include <memory>
#include "TransformHierarchy.h"

namespace ggp
{
	/// <summary>
	/// A transform contains a rotation, euler angles, a scale, and a matrix representation.
	/// It is a wrapper around TransformHierarchy and TransformHierarchy::Handle.
	/// </summary>
	class Transform
	{
	public:
		using Handle = TransformHierarchy::Handle;

		static TransformHierarchy* CreateHierarchySingleton() noexcept;
		static void DestroyHierarchySingleton(TransformHierarchy**) noexcept;
		static Transform Create() noexcept; // explicitly allocate a new transform

		Transform() = delete;
		inline constexpr Transform(Handle h) noexcept : handle(h) {}

		std::optional<Transform> GetFirstChild() noexcept;
		std::optional<Transform> GetNextSibling() noexcept;
		std::optional<Transform> GetParent() noexcept;
		u32 GetChildCount() noexcept;

		// tree modification
		Transform AddChild() noexcept;
		void Destroy() noexcept;

		// matrix calculation (requires some tree traversal unless cached)
		const DirectX::XMFLOAT4X4* GetWorldMatrixPtr() noexcept;
		const DirectX::XMFLOAT4X4* GetWorldInverseTransposeMatrixPtr() noexcept;
		DirectX::XMFLOAT4X4 GetWorldMatrix();
		DirectX::XMFLOAT4X4 GetWorldInverseTransposeMatrix();
	
		// setters
		void SetLocalPosition(float x, float y, float z);
		void SetLocalPosition(DirectX::XMFLOAT3 position);
		inline void TH_VECTORCALL StoreLocalPosition(DirectX::FXMVECTOR pos) noexcept;
		void SetLocalEulerAngles(float x, float y, float z);
		void SetLocalEulerAngles(DirectX::XMFLOAT3 rotation);
		inline void TH_VECTORCALL StoreLocalEulerAngles(DirectX::FXMVECTOR angles) noexcept;
		void SetLocalScale(float x, float y, float z);
		void SetLocalScale(DirectX::XMFLOAT3 scale);
		inline void TH_VECTORCALL StoreLocalScale(DirectX::FXMVECTOR scale) noexcept;
		void SetPosition(float x, float y, float z);
		void SetPosition(DirectX::XMFLOAT3 position);
		inline void TH_VECTORCALL StorePosition(DirectX::FXMVECTOR pos) noexcept;
		void SetEulerAngles(float pitch, float yaw, float roll);
		void SetEulerAngles(DirectX::XMFLOAT3 rotation);
		inline void TH_VECTORCALL StoreEulerAngles(DirectX::FXMVECTOR angles) noexcept;
		void SetScale(float x, float y, float z);
		void SetScale(DirectX::XMFLOAT3 scale);
		inline void TH_VECTORCALL StoreScale(DirectX::FXMVECTOR scale) noexcept;
		
		// getters
		DirectX::XMFLOAT3 GetLocalPosition() const;
		inline DirectX::XMVECTOR LoadLocalPosition() const noexcept;
		DirectX::XMFLOAT3 GetLocalEulerAngles() const;
		inline DirectX::XMVECTOR LoadLocalEulerAngles() const noexcept;
		DirectX::XMFLOAT3 GetLocalScale() const;
		inline DirectX::XMVECTOR LoadLocalScale() const noexcept;
		DirectX::XMFLOAT3 GetPosition() const;
		inline DirectX::XMVECTOR LoadPosition() const noexcept;
		DirectX::XMFLOAT3 GetEulerAngles() const;
		inline DirectX::XMVECTOR LoadEulerAngles() const noexcept;
		DirectX::XMFLOAT3 GetScale() const;
		inline DirectX::XMVECTOR LoadScale() const noexcept;
		DirectX::XMFLOAT3 GetForward() const;
		inline DirectX::XMVECTOR LoadForward() const noexcept;
		DirectX::XMFLOAT3 GetUp() const;
		inline DirectX::XMVECTOR LoadRight() const noexcept;
		DirectX::XMFLOAT3 GetRight() const;
		inline DirectX::XMVECTOR LoadUp() const noexcept;

		// transformers
		void MoveAbsolute(float x, float y, float z);
		void MoveAbsolute(DirectX::XMFLOAT3 offset);
		inline void TH_VECTORCALL MoveAbsoluteVec(DirectX::FXMVECTOR offset) noexcept;
		void MoveAbsoluteLocal(float x, float y, float z);
		void MoveAbsoluteLocal(DirectX::XMFLOAT3 offset);
		inline void TH_VECTORCALL MoveAbsoluteLocalVec(DirectX::FXMVECTOR offset) noexcept;
		void MoveRelative(float x, float y, float z);
		void MoveRelative(DirectX::XMFLOAT3 offset);
		inline void TH_VECTORCALL MoveRelativeVec(DirectX::FXMVECTOR offset) noexcept;
		void RotateLocal(float pitch, float yaw, float roll);
		void RotateLocal(DirectX::XMFLOAT3 rotation);
		inline void TH_VECTORCALL RotateLocalVec(DirectX::FXMVECTOR eulerAngles) noexcept;
		void Scale(float x, float y, float z);
		void Scale(DirectX::XMFLOAT3 scale);
		inline void TH_VECTORCALL ScaleVec(DirectX::FXMVECTOR scale) noexcept;

		/// <summary>
		/// Gets the local translation, rotation, and scale of the transform simultaneously.
		/// </summary>
		/// <param name="outPos">The SIMD register in which to put the position (x, y, z)</param>
		/// <param name="outQuat">The SIMD register in which to put the rotation, as a quaternion (x, y, z, w)</param>
		/// <param name="outScale">The SIMD register in which to put the scale (x, y, z)</param>
		inline void LoadMatrixDecomposed(
			DirectX::XMVECTOR* outPos,
			DirectX::XMVECTOR* outQuat,
			DirectX::XMVECTOR* outScale) const noexcept;

	private:
		TransformHierarchy::Handle handle;
	};

	// global variable for da singleton :)
	namespace internals
	{
		extern ggp::TransformHierarchy* hierarchy;
	}

	// inline function definitions for simd ----------------------------------------------
	inline void Transform::LoadMatrixDecomposed(
		DirectX::XMVECTOR* outPos,
		DirectX::XMVECTOR* outQuat,
		DirectX::XMVECTOR* outScale) const noexcept
	{
		return internals::hierarchy->LoadMatrixDecomposed(handle, outPos, outQuat, outScale);
	}

	inline DirectX::XMVECTOR Transform::LoadLocalPosition() const noexcept
	{
		return internals::hierarchy->LoadLocalPosition(handle);
	}

	inline DirectX::XMVECTOR Transform::LoadLocalEulerAngles() const noexcept
	{
		return internals::hierarchy->LoadLocalEulerAngles(handle);
	}

	inline DirectX::XMVECTOR Transform::LoadLocalScale() const noexcept
	{
		return internals::hierarchy->LoadLocalScale(handle);
	}

	inline DirectX::XMVECTOR Transform::LoadPosition() const noexcept
	{
		return internals::hierarchy->LoadPosition(handle);
	}

	inline DirectX::XMVECTOR Transform::LoadEulerAngles() const noexcept
	{
		return internals::hierarchy->LoadEulerAngles(handle);
	}

	inline DirectX::XMVECTOR Transform::LoadScale() const noexcept
	{
		return internals::hierarchy->LoadScale(handle);
	}

	inline void TH_VECTORCALL Transform::StorePosition(DirectX::FXMVECTOR pos) noexcept
	{
		internals::hierarchy->StorePosition(handle, pos);
	}

	inline void TH_VECTORCALL Transform::StoreEulerAngles(DirectX::FXMVECTOR angles) noexcept
	{
		internals::hierarchy->StoreEulerAngles(handle, angles);
	}

	inline void TH_VECTORCALL Transform::StoreScale(DirectX::FXMVECTOR scale) noexcept
	{
		internals::hierarchy->StoreScale(handle, scale);
	}

	inline void TH_VECTORCALL Transform::StoreLocalPosition(DirectX::FXMVECTOR pos) noexcept
	{
		internals::hierarchy->StoreLocalPosition(handle, pos);
	}

	inline void TH_VECTORCALL Transform::StoreLocalEulerAngles(DirectX::FXMVECTOR angles) noexcept
	{
		internals::hierarchy->StoreLocalEulerAngles(handle, angles);
	}

	inline void TH_VECTORCALL Transform::StoreLocalScale(DirectX::FXMVECTOR scale) noexcept
	{
		internals::hierarchy->StoreLocalScale(handle, scale);
	}

	inline DirectX::XMVECTOR Transform::LoadForward() const noexcept
	{
		using namespace DirectX;
		XMVECTOR out;
		XMVECTOR quat;
		XMVECTOR scale;
		LoadMatrixDecomposed(&out, &quat, &scale);
		out = XMVectorSet(0, 0, 1, 0);
		return XMVector3Rotate(out, quat);
	}

	inline DirectX::XMVECTOR Transform::LoadRight() const noexcept
	{	
		using namespace DirectX;
		XMVECTOR out;
		XMVECTOR quat;
		XMVECTOR scale;
		LoadMatrixDecomposed(&out, &quat, &scale);
		out = XMVectorSet(1, 0, 0, 0);
		return XMVector3Rotate(out, quat);
	}

	inline DirectX::XMVECTOR Transform::LoadUp() const noexcept
	{
		using namespace DirectX;
		XMVECTOR out;
		XMVECTOR quat;
		XMVECTOR scale;
		LoadMatrixDecomposed(&out, &quat, &scale);
		out = XMVectorSet(0, 1, 0, 0);
		return XMVector3Rotate(out, quat);
	}

	inline void TH_VECTORCALL Transform::ScaleVec(DirectX::FXMVECTOR scale) noexcept
	{
		using namespace DirectX;
		StoreLocalScale(XMVectorMultiply(scale, LoadLocalScale()));
	}

	inline void TH_VECTORCALL Transform::RotateLocalVec(DirectX::FXMVECTOR eulerAngles) noexcept
	{
		using namespace DirectX;
		// convert to quat
		XMVECTOR current = XMQuaternionRotationRollPitchYawFromVector(LoadLocalEulerAngles());
		XMVECTOR diff = XMQuaternionRotationRollPitchYawFromVector(eulerAngles);

		// actual rotation
		current = XMQuaternionMultiply(diff, current);

		// store, quattoeuler doesnt use simd
		XMFLOAT4 q;
		XMStoreFloat4(&q, current);
		SetLocalEulerAngles(QuatToEuler(q));
	}

	inline void TH_VECTORCALL Transform::MoveAbsoluteLocalVec(DirectX::FXMVECTOR offset) noexcept
	{
		StoreLocalPosition(DirectX::XMVectorAdd(offset, LoadLocalPosition()));
	}

	inline void TH_VECTORCALL Transform::MoveAbsoluteVec(DirectX::FXMVECTOR offset) noexcept
	{
		using namespace DirectX;
		// performing load, which traverses up parents, and then also a set, which traverses children to set the dirty flag
		XMVECTOR pos = LoadPosition();
		XMVECTOR expected = XMVectorAdd(offset, pos);
		StorePosition(expected);
#if defined(DEBUG) || defined(_DEBUG)
		XMVECTOR actual = LoadPosition();
		// make sure the difference between the two is small
		XMVECTOR diff = XMVector3LengthSq(XMVectorSubtract(expected, actual));
		gassert(XMVectorGetX(diff) < 0.001f);
#endif
	}

	inline void TH_VECTORCALL Transform::MoveRelativeVec(DirectX::FXMVECTOR offset) noexcept
	{
		using namespace DirectX;
		XMVECTOR pos;
		XMVECTOR rotated;
		XMVECTOR scale;
		LoadMatrixDecomposed(&pos, &rotated, &scale);
		// since LoadMatrix gives the global rotation, we are rotating the global space offset into our local space
		rotated = XMVector3Rotate(offset, rotated);
		// rotated value already in global space, so it can be applied directly to our local position and it will still be global
		MoveAbsoluteLocalVec(rotated);
	}
}