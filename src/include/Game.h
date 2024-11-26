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
		void CreateSamplers();
		void CreateMaterials();
		void LoadMeshes();
		void LoadCubemapAndCreateSkybox();
		void CreateEntities();
		void UIBeginFrame(float deltaTime) noexcept;
		void UIEndFrame() noexcept;
		void BuildUI() noexcept;

		bool m_spinningEnabled = false;
		std::array<float, 4> m_backgroundColor = { 0 };

		ggp::dict<std::unique_ptr<ggp::Mesh>> m_meshes;
		ggp::dict<std::unique_ptr<Material>> m_materials;
		ggp::dict<ggp::com_p<ID3D11ShaderResourceView>> m_textureViews;
		ggp::com_p<ID3D11SamplerState> m_defaultSampler;
		std::vector<Entity> m_entities;
		ggp::TransformHierarchy* m_transformHierarchy;

		size_t m_activeCamera;
		std::vector<std::shared_ptr<Camera>> m_cameras;

		std::unique_ptr<SimpleVertexShader> m_vertexShader;
		std::unique_ptr<SimplePixelShader> m_pixelShader;

		std::unique_ptr<std::array<Light, MAX_LIGHTS>> m_lights;
		Sky::SharedResources m_skyboxResources;
		std::unique_ptr<Sky> m_skybox;
	};
}

