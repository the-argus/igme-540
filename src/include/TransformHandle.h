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
		size_t GetNumChildren() const noexcept;
		TransformHandle GetChild(size_t idx) const noexcept;
		std::optional<TransformHandle> GetParent() const noexcept;

		// tree modification
		TransformHandle AddChild() const noexcept;
		void RemoveChildAt(size_t idx) const noexcept;

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
		explicit inline TransformHandle(u32 raw, u8 extra) noexcept : id(raw), extra(extra) {};

		u32 id;
		u8 extra;
	};
}