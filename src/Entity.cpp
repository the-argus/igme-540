#include "Entity.h"
#include "Graphics.h"

ggp::Entity::Entity(Mesh* mesh) noexcept
	: m_mesh(mesh),
	m_transform(Transform::Create())
{
}

ggp::Entity::Entity(Mesh* mesh, Transform transform) noexcept
	: m_mesh(mesh),
	m_transform(transform)
{
}
