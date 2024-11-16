#include "Entity.h"
#include "Graphics.h"

ggp::Entity::Entity(Mesh* mesh, Material* material, const char* debugName) noexcept
	: m_mesh(mesh),
	m_debugName(debugName),
	m_material(material),
	m_transform(Transform::Create())
{
}

ggp::Entity::Entity(Mesh* mesh, Material* material, Transform transform, const char* debugName) noexcept
	: m_mesh(mesh),
	m_debugName(debugName),
	m_material(material),
	m_transform(transform)
{
}
