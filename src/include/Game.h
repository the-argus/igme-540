#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

#include "SimpleShader.h"
#include "Material.h"
#include "Mesh.h"
#include "Entity.h"
#include "Camera.h"
#include "Light.h"
#include "Sky.h"
#include "ggp_com_pointer.h"
#include "ggp_dict.h"

namespace ggp
{
	class Game
	{
	public:
		Game() = default;
		~Game();
		Game(const Game&) = delete;
		Game& operator=(const Game&) = delete;

		void Initialize();
		void Update(float deltaTime, float totalTime);
		void Draw(float deltaTime, float totalTime);
		void OnResize();

	private:

		void LoadShaders();
		void LoadTextures();
		void CreateShadowMaps();
		void CreateSamplers();
		void CreateRenderTarget();
		void CreateMaterials();
		void LoadMeshes();
		void LoadCubemapAndCreateSkybox();
		void CreateEntities();
		void UIBeginFrame(float deltaTime) noexcept;
		void UIEndFrame() noexcept;
		void BuildUI() noexcept;
		void UpdatePortalLocations() noexcept;
		void RenderSceneAndPortals(const Camera& camera, float delta, float total) noexcept;
		void RenderShadowMaps() noexcept;
		void RenderSceneFull(const Camera& camera, float deltaTime, float totalTime) noexcept;

		bool m_spinningEnabled = true;
		std::array<float, 4> m_backgroundColor = { 0 };

		dict<std::unique_ptr<Mesh>> m_meshes;
		dict<std::unique_ptr<Material>> m_materials;
		dict<com_p<ID3D11ShaderResourceView>> m_textureViews;
		com_p<ID3D11SamplerState> m_defaultSampler;
		std::vector<Entity> m_entities;
		TransformHierarchy* m_transformHierarchy;

		size_t m_activeCamera;
		std::vector<std::shared_ptr<Camera>> m_cameras;

		std::unique_ptr<SimpleVertexShader> m_vertexShader;
		std::unique_ptr<SimplePixelShader> m_pixelShader;

		// shared by all post processes
		std::unique_ptr<SimpleVertexShader> m_postProcessVertexShader;
		com_p<ID3D11SamplerState> m_postProcessSamplerState;
		// per-postprocess pass resources (only one pass, atm)
		std::unique_ptr<SimplePixelShader> m_postProcessPixelShader;
		i32 m_blurRadius = 0;

		std::unique_ptr<std::array<Light, MAX_LIGHTS>> m_lights;
		static constexpr int shadowMapResolution = 2048;
		std::unique_ptr<std::array<std::optional<ShadowMapResources>, MAX_LIGHTS>> m_shadowMapResources;
		std::vector<com_p<ID3D11ShaderResourceView>> m_shadowMaps;
		std::vector<com_p<ID3D11DepthStencilView>> m_shadowMapDepths;
		com_p<ID3D11RasterizerState> m_shadowMapRasterizerState;
		com_p<ID3D11SamplerState> m_shadowMapSamplerState;
		std::unique_ptr<SimpleVertexShader> m_shadowMapVertexShader;

		Sky::SharedResources m_skyboxResources;
		std::unique_ptr<Sky> m_skybox;

		static constexpr u64 portalCount = 2;
		struct Portal
		{
			std::optional<u64> connectedPortalIndex; // null if no connection, render as black
			com_p<ID3D11RenderTargetView> rtv;
			com_p<ID3D11ShaderResourceView> srv;
			com_p<ID3D11DepthStencilView> dsv;
			std::unique_ptr<Camera> camera;
			// where to draw our render target
			std::optional<Transform> transform;
		};

		std::unique_ptr<SimplePixelShader> m_portalShader;
		std::array<Portal, portalCount> m_portals;
		// std::array<ID3D11RenderTargetView*, 1 + portalCount> m_portalRenderTargets;
		std::array<D3D11_VIEWPORT, 1 + portalCount> m_portalViewports;

		// views of only the first texture in the texture2D array
		com_p<ID3D11DepthStencilView> m_singleDepthStencilView;
		com_p<ID3D11RenderTargetView> m_singleRenderTargetView; // draw portals to this
		// view for sampling rendered scene
		com_p<ID3D11ShaderResourceView> m_singleShaderResourceView;
	};
}

