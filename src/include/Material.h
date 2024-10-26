#pragma once

#include <DirectXMath.h>

#include "SimpleShader.h"

#include "short_numbers.h"

namespace ggp
{
	class Material
	{
	public:
		struct Options
		{
			DirectX::XMFLOAT4 colorRGBA = {1.F, 1.F, 1.F, 1.F};
			f32 roughness = 0.f;
		};

		// materials always are constructed with valid color and shaders
		Material() = delete;
		Material(SimpleVertexShader* vertexShader, SimplePixelShader* pixelShader, const Options&) noexcept;

		Material(const Material&) = default;
		Material& operator=(const Material&) = default;
		Material(Material&&) = default;
		Material& operator=(Material&&) = default;

		inline DirectX::XMFLOAT4 GetColor() const { return m_data.colorRGBA; };
		inline f32 GetRoughness() const { return m_data.roughness; };
		inline SimpleVertexShader* GetVertexShader() const { return m_vertexShader; };
		inline SimplePixelShader* GetPixelShader() const { return m_pixelShader; };

		inline void SetColor(DirectX::XMFLOAT4 color) { m_data.colorRGBA = color; }
		inline void SetColor(f32 roughness) { m_data.roughness = roughness; }

	private:
		Options m_data;
		SimpleVertexShader* m_vertexShader;
		SimplePixelShader* m_pixelShader;
	};
}
