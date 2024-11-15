#pragma once

#include "Mesh.h"
#include "SimpleShader.h"
#include "ggp_com_pointer.h"
#include <memory>

namespace ggp
{
	class Sky
	{
	public:
		// resources used by every skybox. these have a separate lifetime than an
		// induvidual skybox, probably across the whole game rather than a scene
		struct SharedResources
		{
			com_p<ID3D11DepthStencilState> depthStencilState;
			com_p<ID3D11RasterizerState> rasterizerState;
			Mesh* skyMesh;
			std::unique_ptr<SimplePixelShader> skyboxPixelShader;
			std::unique_ptr<SimpleVertexShader> skyboxVertexShader;
		};

		Sky() = delete;
		Sky(ID3D11ShaderResourceView* skyboxCubemapTextureView, ID3D11SamplerState* skyboxSampler) noexcept;

		void Draw(const SharedResources& skyboxResources, DirectX::XMFLOAT4X4 viewMatrix, DirectX::XMFLOAT4X4 projectionMatrix) const noexcept;

		static com_p<ID3D11DepthStencilState> CreateDepthStencilStateThatKeepsDeepPixels() noexcept;
		static com_p<ID3D11RasterizerState> CreateRasterizerStateThatDrawsBackfaces() noexcept;


		struct LoadCubemapOptions {
			const wchar_t* right;
			const wchar_t* left;
			const wchar_t* up;
			const wchar_t* down;
			const wchar_t* front;
			const wchar_t* back;
		};
		static com_p<ID3D11ShaderResourceView> LoadCubemap(const LoadCubemapOptions& options);

	private:
		ID3D11SamplerState* m_sampler;
		ID3D11ShaderResourceView* m_cubemap;
	};
}