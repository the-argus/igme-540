#include "Entity.h"
#include "Graphics.h"

ggp::Entity::Entity(Mesh* mesh, Material* material) noexcept
	: m_mesh(mesh),
	m_material(material),
	m_transform(Transform::Create())
{
}

ggp::Entity::Entity(Mesh* mesh, Material* material, Transform transform) noexcept
	: m_mesh(mesh),
	m_material(material),
	m_transform(transform)
{
}
