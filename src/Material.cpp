#include "Material.h"
#include "errors.h"

using namespace ggp;

Material::Material(Options&& options, SimpleVertexShader* vertexShader, SimplePixelShader* pixelShader) noexcept :
	m_data(std::move(options)),
	m_vertexShader(vertexShader ? vertexShader : defaultVertexShader),
	m_pixelShader(pixelShader ? pixelShader : defaultPixelShader)
{
	m_data.samplerState = m_data.samplerState ? m_data.samplerState : defaultSamplerState.Get();
	m_data.albedoTextureView = m_data.albedoTextureView ? m_data.albedoTextureView : defaultAlbedoTextureView.Get();
	m_data.specularTextureView = m_data.specularTextureView ? m_data.specularTextureView : defaultSpecularTextureView.Get();
	m_data.normalTextureView = m_data.normalTextureView ? m_data.normalTextureView : defaultNormalTextureView.Get();
}

void Material::BindTextureViewsAndSamplerStates(const ShaderVariableNames& varnames) const
{
	gassert(varnames.albedoTexture);
	gassert(m_data.albedoTextureView);
	m_pixelShader->SetShaderResourceView(varnames.albedoTexture, m_data.albedoTextureView);
	gassert(varnames.normalTexture);
	gassert(m_data.normalTextureView);
	m_pixelShader->SetShaderResourceView(varnames.normalTexture, m_data.normalTextureView);
	gassert(varnames.specularTexture);
	gassert(m_data.specularTextureView);
	m_pixelShader->SetShaderResourceView(varnames.specularTexture, m_data.specularTextureView);

	gassert(varnames.sampler);
	gassert(m_data.samplerState);
	m_pixelShader->SetSamplerState(varnames.sampler, m_data.samplerState);
}
