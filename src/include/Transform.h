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

		Transform() = delete;
		inline constexpr Transform(Handle h) noexcept : handle(h) {}

		std::optional<Transform> GetFirstChild() noexcept;
		std::optional<Transform> GetNextSibling() noexcept;
		std::optional<Transform> GetParent() noexcept;

		// tree modification
		Transform AddChild() noexcept;
		void Destroy() noexcept;

		const DirectX::XMFLOAT4X4* GetMatrix() noexcept;

		// the only const functions are the loads, a const transform is pretty useless
		inline void LoadMatrixDecomposed(
			DirectX::XMVECTOR* outPos,
			DirectX::XMVECTOR* outQuat,
			DirectX::XMVECTOR* outScale) const noexcept;
		inline DirectX::XMVECTOR LoadLocalPosition() const noexcept;
		inline DirectX::XMVECTOR LoadLocalEulerAngles() const noexcept;
		inline DirectX::XMVECTOR LoadLocalScale() const noexcept;
		inline DirectX::XMVECTOR LoadPosition() const noexcept;
		inline DirectX::XMVECTOR LoadEulerAngles() const noexcept;
		inline DirectX::XMVECTOR LoadScale() const noexcept;

		// store local space or global space
		inline void TH_VECTORCALL StorePosition(DirectX::FXMVECTOR pos) noexcept;
		inline void TH_VECTORCALL StoreEulerAngles(DirectX::FXMVECTOR angles) noexcept;
		inline void TH_VECTORCALL StoreScale(DirectX::FXMVECTOR scale) noexcept;
		inline void TH_VECTORCALL StoreLocalPosition(DirectX::FXMVECTOR pos) noexcept;
		inline void TH_VECTORCALL StoreLocalEulerAngles(DirectX::FXMVECTOR angles) noexcept;
		inline void TH_VECTORCALL StoreLocalScale(DirectX::FXMVECTOR scale) noexcept;

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
		return internals::hierarchy->LoadScale(handle);
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
}