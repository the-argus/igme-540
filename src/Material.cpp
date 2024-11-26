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
	if (!m_data.metalnessTextureView)
	{
		m_data.metalnessTextureView = m_data.isMetal
			? defaultMetalnessTextureViewMetal.Get()
			: defaultMetalnessTextureViewNonMetal.Get();
	}
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

	gassert(varnames.metalnessTexture);
	gassert(m_data.metalnessTextureView);
	m_pixelShader->SetShaderResourceView(varnames.metalnessTexture, m_data.metalnessTextureView);

	m_pixelShader->SetInt(varnames.roughnessEnabledInt, m_data.roughnessTextureView == nullptr);
	if (m_data.roughnessTextureView)
	{
		gassert(varnames.roughnessTexture);
		gassert(m_data.roughnessTextureView);
		m_pixelShader->SetShaderResourceView(varnames.metalnessTexture, m_data.metalnessTextureView);
	}
	else
	{
		m_pixelShader->SetFloat(varnames.roughness, m_data.roughness);
	}
	
	gassert(varnames.sampler);
	gassert(m_data.samplerState);
	m_pixelShader->SetSamplerState(varnames.sampler, m_data.samplerState);
}
