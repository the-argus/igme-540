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
	// TODO: fallback textures for things
	for (const auto& pair : m_data.textureViews) {
		gassert(pair.second);
	}
#endif
}

auto Material::PtrForSlot(TextureSlot slot) noexcept -> ID3D11ShaderResourceView*&
{
	switch (slot) {
	case TextureSlot::Albedo:
		return m_data.albedoTextureView;
	case TextureSlot::Normal:
		return m_data.normalTextureView;
	case TextureSlot::Specular:
		return m_data.specularTextureView;
	}
}

void Material::BindSRVsAndSamplerStates() const
{
	m_pixelShader->SetShaderResourceView("albedoTexture", m_data.albedoTextureView);
	m_pixelShader->SetShaderResourceView("normalTexture", m_data.normalTextureView);
	m_pixelShader->SetShaderResourceView("specularTexture", m_data.specularTextureView);

	for (const auto& samplerState : m_data.samplerStates) {
		m_pixelShader->SetSamplerState(samplerState.first, samplerState.second);
	}
}
