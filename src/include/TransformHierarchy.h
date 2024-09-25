#pragma once

#include <DirectXMath.h>

#include "BlockAllocator.h"
#include <vector>
#include <optional>

namespace ggp
{
	class TransformHierarchy
	{
	public:
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

		private:
			u32 id;
		};

		class InternalTransform
		{
		public:
			InternalTransform() noexcept;
		private:
			DirectX::XMFLOAT4A localPosition; // fourth element of position is dirty flag
			DirectX::XMFLOAT3A localRotation;
			DirectX::XMFLOAT4A quaternion;
			DirectX::XMFLOAT3A localScale;

			inline bool IsDirty() const noexcept { return localPosition.w > 0; }
			inline void SetDirty(bool value) noexcept { localPosition.w = value ? -1 : 1; }
		};

		TransformHandle InsertRootTransform() noexcept;

	private:
		// transforms with no parent
		std::vector<InternalTransform*> m_rootTransforms;
		// matrices, calculated/updated each frame
		BlockAllocator m_matrixCache;
		// transforms with no children
		BlockAllocator m_transformAllocator;
		// transforms with 1-4 children
		BlockAllocator m_transformFamilyAllocator;
		// transforms with a ton of siblings need to be heap allocated and so have to be freed on destruction
		std::vector<InternalTransform*> m_bigTransforms;
	};
}
