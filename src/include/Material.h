#pragma once

#include <DirectXMath.h>

#include "SimpleShader.h"

namespace ggp
{
	class Material
	{
	public:
		// materials always are constructed with valid color and shaders
		Material() = delete;
		Material(SimpleVertexShader* vertexShader, SimplePixelShader* pixelShader, DirectX::XMFLOAT4 color = {1.F, 1.F, 1.F, 1.F}) noexcept;

		Material(const Material&) = default;
		Material& operator=(const Material&) = default;
		Material(Material&&) = default;
		Material& operator=(Material&&) = default;

		inline DirectX::XMFLOAT4 GetColor() const { return m_color; };
		inline SimpleVertexShader* GetVertexShader() const { return m_vertexShader; };
		inline SimplePixelShader* GetPixelShader() const { return m_pixelShader; };

		inline void SetColor(DirectX::XMFLOAT4 color) { m_color = color; }

	private:
		DirectX::XMFLOAT4 m_color;
		SimpleVertexShader* m_vertexShader;
		SimplePixelShader* m_pixelShader;
	};
}
