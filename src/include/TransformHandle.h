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
		static void DestroyHierarchySingleton(TransformHierarchy*) noexcept;

		Transform() = delete;
		inline constexpr Transform(Handle h) noexcept : handle(h) {}

		// tree traversal
		std::optional<Handle> GetFirstChild() const noexcept;
		std::optional<Handle> GetNextSibling() const noexcept;
		std::optional<Handle> GetParent() const noexcept;

		// tree modification
		Handle AddChild() const noexcept;
		void Destroy() const noexcept; // NOTE: after this, this transform handle is invalid

		// getters- always okay to call these because they are entirely local to this
		// transform and have nothing to do with the heirarchy
		DirectX::XMFLOAT3A GetLocalPosition() const noexcept;
		DirectX::XMFLOAT3A GetLocalEulerAngles() const noexcept;
		DirectX::XMFLOAT3A GetLocalScale() const noexcept;

		// getters- these call into the transform hierarchy and require updates
		DirectX::XMFLOAT4X4 GetMatrix() const noexcept;
		DirectX::XMFLOAT3A GetPosition() const noexcept;
		DirectX::XMFLOAT3A GetEulerAngles() const noexcept;
		DirectX::XMFLOAT3A GetScale() const noexcept;

		// setters- cause updates in the transform hierarchy
		void SetPosition(const DirectX::XMFLOAT3A& pos) const noexcept;
		void SetEulerAngles(const DirectX::XMFLOAT3A& angles) const noexcept;
		void SetScale(const DirectX::XMFLOAT3A& scale) const noexcept;
		void SetLocalPosition(const DirectX::XMFLOAT3A& pos) const noexcept;
		void SetLocalEulerAngles(const DirectX::XMFLOAT3A& angles) const noexcept;
		void SetLocalScale(const DirectX::XMFLOAT3A& scale) const noexcept;

	private:
		TransformHierarchy::Handle handle;
	};
}