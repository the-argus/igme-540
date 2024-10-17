#include "Material.h"

ggp::Material::Material(SimpleVertexShader* vertexShader, SimplePixelShader* pixelShader, DirectX::XMFLOAT4 color) noexcept
	:m_color(color),
	m_vertexShader(vertexShader),
	m_pixelShader(pixelShader)
{
}
