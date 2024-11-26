#pragma once

#include <DirectXMath.h>

#include "SimpleShader.h"

#include "short_numbers.h"
#include "errors.h"
#include "ggp_com_pointer.h"

namespace ggp
{
	extern com_p<ID3D11SamplerState> defaultSamplerState;
	extern com_p<ID3D11ShaderResourceView> defaultAlbedoTextureView;
	extern com_p<ID3D11ShaderResourceView> defaultNormalTextureView;
	extern com_p<ID3D11ShaderResourceView> defaultMetalnessTextureViewMetal;
	extern com_p<ID3D11ShaderResourceView> defaultMetalnessTextureViewNonMetal;
	extern SimpleVertexShader* defaultVertexShader;
	extern SimplePixelShader* defaultPixelShader;

	class Material
	{
	public:
		struct Options
		{
			DirectX::XMFLOAT4 colorRGBA = { 1.F, 1.F, 1.F, 1.F };
			f32 roughness = 1.f; // only takes effect if no roughness texture
			i32 isMetal; // boolean, if not providing a metalness texture
			DirectX::XMFLOAT2 uvOffset;
			DirectX::XMFLOAT2 uvScale = { 1.F, 1.F };

			ID3D11SamplerState* samplerState;
			ID3D11ShaderResourceView* albedoTextureView;
			ID3D11ShaderResourceView* normalTextureView;
			ID3D11ShaderResourceView* roughnessTextureView;
			ID3D11ShaderResourceView* metalnessTextureView;
		};

		struct ShaderVariableNames
		{
			const char* sampler;
			const char* albedoTexture;
			const char* normalTexture;
			const char* roughnessTexture;
			const char* metalnessTexture;
			const char* roughnessEnabledInt;
			const char* roughness;
		};

		// materials always are constructed with valid color and shaders
		Material() = delete;
		// vertex and pixel shader are optional, can fall back to defaults as long as a ggp::Game exists
		Material(Options&&, SimpleVertexShader* vertexShader = nullptr, SimplePixelShader* pixelShader = nullptr) noexcept;

		Material(const Material&) = default;
		Material& operator=(const Material&) = default;
		Material(Material&&) = default;
		Material& operator=(Material&&) = default;

		void BindTextureViewsAndSamplerStates(const ShaderVariableNames&) const;

		inline DirectX::XMFLOAT4 GetColor() const { return m_data.colorRGBA; };
		inline DirectX::XMFLOAT2 GetUVScale() const { return m_data.uvScale; };
		inline DirectX::XMFLOAT2 GetUVOffset() const { return m_data.uvOffset; };
		inline f32 GetRoughness() const { return m_data.roughness; };
		inline SimpleVertexShader* GetVertexShader() const { return m_vertexShader; };
		inline SimplePixelShader* GetPixelShader() const { return m_pixelShader; };

	private:
		Options m_data;
		SimpleVertexShader* m_vertexShader;
		SimplePixelShader* m_pixelShader;
	};
}
