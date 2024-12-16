#pragma once

#include "Transform.h"
#include "Mesh.h"
#include "Material.h"
#include "Variant.h"
#include "ggp_dict.h"

namespace ggp
{
	class Entity
	{
	public:
		Entity() = delete;
		explicit Entity(Mesh* mesh, Material* material, std::string&& debugName = "UNKNOWN") noexcept;
		Entity(Mesh* mesh, Material* material, Transform transform, std::string&& debugName = "UNKNOWN") noexcept;
		Entity(Mesh* mesh, Material* material, Transform transform, dict<Variant>&& properties, std::string&& debugName = "UNKNOWN") noexcept;

		inline Mesh* GetMesh() const noexcept { return m_mesh; }
		inline Material* GetMaterial() const noexcept { return m_material; }

		// Transform is actually a reference type internally, so bc this is like Transform& this method is not const
		inline Transform GetTransform() noexcept { return m_transform; }

		inline const char* GetDebugName() const noexcept { return m_debugName.c_str(); }

		inline const dict<Variant>& GetProperties() const noexcept { return m_properties; }
		inline dict<Variant>& GetProperties() noexcept { return m_properties; }

	private:
		std::string m_debugName;
		Mesh* m_mesh;
		Material* m_material;
		Transform m_transform;
		// properties as read from .map file, if spawned from there
		dict<Variant> m_properties;
	};
}