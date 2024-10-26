#include "Material.h"

using namespace ggp;

Material::Material(SimpleVertexShader* vertexShader, SimplePixelShader* pixelShader, const Options& options) noexcept :
	m_data(options),
	m_vertexShader(vertexShader),
	m_pixelShader(pixelShader)
{
}
