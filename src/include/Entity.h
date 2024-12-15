#pragma once

#include "Transform.h"
#include "Mesh.h"
#include "Material.h"

namespace ggp
{
	class Entity
	{
	public:
		Entity() = delete;
		explicit Entity(Mesh* mesh, Material* material, std::string&& debugName = "UNKNOWN") noexcept;
		Entity(Mesh* mesh, Material* material, Transform transform, std::string&& debugName = "UNKNOWN") noexcept;

		inline Mesh* GetMesh() const noexcept { return m_mesh; }
		inline Material* GetMaterial() const noexcept { return m_material; }

		// Transform is actually a reference type internally, so bc this is like Transform& this method is not const
		inline Transform GetTransform() noexcept { return m_transform; }

		inline const char* GetDebugName() const noexcept { return m_debugName.c_str(); }

	private:
		std::string m_debugName;
		Mesh* m_mesh;
		Material* m_material;
		Transform m_transform;
	};
}