#include "Material.h"
#include "errors.h"

using namespace ggp;

Material::Material(SimpleVertexShader* vertexShader, SimplePixelShader* pixelShader, Options&& options) noexcept :
	m_data(std::move(options)),
	m_vertexShader(vertexShader),
	m_pixelShader(pixelShader)
{
#if defined(_DEBUG) || defined(DEBUG)
	for (const auto& pair : m_data.samplerStates) {
		gassert(pair.second);
	}
	for (const auto& pair : m_data.textureViews) {
		gassert(pair.second);
	}
#endif
}

void Material::AddTextureSRV(const char* name, const ggp::com_p<ID3D11ShaderResourceView>& srv)
{
	gassert(!m_data.textureViews.contains(name));
	gassert(srv);
	gassert(name);
	m_data.textureViews.insert({name, srv});
}

void Material::AddSampler(const char* name, const ggp::com_p<ID3D11SamplerState>& sampler)
{
	gassert(!m_data.samplerStates.contains(name));
	gassert(sampler);
	gassert(name);
	m_data.samplerStates.insert({ name, sampler });
}

void Material::BindSRVsAndSamplerStates() const
{
	for (const auto& texView : m_data.textureViews) {
		m_pixelShader->SetShaderResourceView(texView.first, texView.second);
	}
	for (const auto& samplerState : m_data.samplerStates) {
		m_pixelShader->SetSamplerState(samplerState.first, samplerState.second);
	}
}
