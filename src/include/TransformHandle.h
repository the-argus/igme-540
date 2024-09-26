#pragma once
#include <DirectXMath.h>
#include <optional>

namespace ggp
{
	class TransformHierarchy;
	class TransformHandle
	{
	public:
		// tree traversal
		std::optional<TransformHandle> GetFirstChild() const noexcept;
		std::optional<TransformHandle> GetNextSibling() const noexcept;
		std::optional<TransformHandle> GetParent() const noexcept;

		// tree modification
		TransformHandle AddChild() const noexcept;
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

		friend class TransformHierarchy;

	private:
		explicit inline TransformHandle(u32 raw, TransformHierarchy* hierarchy) noexcept : id(raw), hierarchy(hierarchy) {};

		TransformHierarchy* hierarchy;
		u32 id;
	};
}