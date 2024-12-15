#include "Entity.h"
#include "Graphics.h"

ggp::Entity::Entity(Mesh* mesh, Material* material, std::string&& debugName) noexcept
	: m_mesh(mesh),
	m_debugName(std::move(debugName)),
	m_material(material),
	m_transform(Transform::Create())
{
}

ggp::Entity::Entity(Mesh* mesh, Material* material, Transform transform, std::string&& debugName) noexcept
	: m_mesh(mesh),
	m_debugName(std::move(debugName)),
	m_material(material),
	m_transform(transform)
{
}
