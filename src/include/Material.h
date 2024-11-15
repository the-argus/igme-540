#pragma once

#include <DirectXMath.h>

#include "SimpleShader.h"

#include "short_numbers.h"
#include "ggp_dict.h"
#include "ggp_com_pointer.h"

namespace ggp
{
	extern ID3D11SamplerState* defaultSamplerState;
	extern ID3D11ShaderResourceView* defaultNormalTextureView;
	extern ID3D11ShaderResourceView* defaultAlbedoTextureView;
	extern ID3D11ShaderResourceView* defaultSpecularTextureView;

	class Material
	{
	public:
		enum TextureSlot: u8
		{
			Albedo,
			Normal,
			Specular,
		};

		struct Options
		{
			DirectX::XMFLOAT4 colorRGBA = { 1.F, 1.F, 1.F, 1.F };
			f32 roughness = 0.f;
			DirectX::XMFLOAT2 uvOffset;
			DirectX::XMFLOAT2 uvScale = { 1.F, 1.F };

			ID3D11SamplerState* samplerState;
			ID3D11ShaderResourceView* albedoTextureView;
			ID3D11ShaderResourceView* normalTextureView;
			ID3D11ShaderResourceView* specularTextureView;
		};

		// materials always are constructed with valid color and shaders
		Material() = delete;
		Material(SimpleVertexShader* vertexShader, SimplePixelShader* pixelShader, Options&&) noexcept;

		Material(const Material&) = default;
		Material& operator=(const Material&) = default;
		Material(Material&&) = default;
		Material& operator=(Material&&) = default;

		void BindSRVsAndSamplerStates() const;

		inline DirectX::XMFLOAT4 GetColor() const { return m_data.colorRGBA; };
		inline DirectX::XMFLOAT2 GetUVScale() const { return m_data.uvScale; };
		inline DirectX::XMFLOAT2 GetUVOffset() const { return m_data.uvOffset; };
		inline f32 GetRoughness() const { return m_data.roughness; };
		inline SimpleVertexShader* GetVertexShader() const { return m_vertexShader; };
		inline SimplePixelShader* GetPixelShader() const { return m_pixelShader; };

		inline void SetColor(DirectX::XMFLOAT4 color) { m_data.colorRGBA = color; }
		inline void SetColor(f32 roughness) { m_data.roughness = roughness; }

	private:
		ID3D11ShaderResourceView*& PtrForSlot(TextureSlot) noexcept;

		Options m_data;
		SimpleVertexShader* m_vertexShader;
		SimplePixelShader* m_pixelShader;
	};
}
