#pragma once

#include "Transform.h"
#include "Mesh.h"

namespace ggp
{
	class Entity
	{
	public:
		Entity() = delete;
		explicit Entity(Mesh* mesh) noexcept;
		Entity(Mesh* mesh, Transform transform) noexcept;

		inline Mesh* GetMesh() const noexcept { return m_mesh; }

		// Transform is actually a reference type internally, so bc this is like Transform& this method is not const
		inline Transform GetTransform() noexcept { return m_transform; }

	private:
		Mesh* m_mesh;
		Transform m_transform;
	};
}