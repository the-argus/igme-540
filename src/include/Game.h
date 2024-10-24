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
#include "ggp_com_pointer.h"

class Game
{
public:
	// Basic OOP setup
	Game() = default;
	~Game();
	Game(const Game&) = delete; // Remove copy constructor
	Game& operator=(const Game&) = delete; // Remove copy-assignment operator

	// Primary functions
	void Initialize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	void OnResize();

private:

	// Initialization helper methods - feel free to customize, combine, remove, etc.
	void LoadShaders();
	void CreateGeometry();
	void CreateEntities();
	void UIBeginFrame(float deltaTime) noexcept;
	void UIEndFrame() noexcept;
	void BuildUI() noexcept;

	bool m_spinningEnabled = false;
	std::array<float, 4> m_backgroundColor = { 0 };
	// NOTE: number of samples is tied to the percieved scroll speed of the graph,
	// which also moves at a frame dependent speed
	static constexpr size_t numframerateSamples = 5000;
	std::unique_ptr<std::array<float, numframerateSamples>> m_framerateHistory;
	// NOTE: imgui uses ascii, no wstring
	std::vector<std::string> m_lastTypedStrings;
	std::string m_nextStringUp;
	static constexpr size_t maxStrings = 3;
	bool m_demoWindowVisible = false;

	std::unordered_map<std::wstring, ggp::Mesh> m_alwaysLoadedMeshes;
	std::vector<std::unique_ptr<ggp::Material>> m_materials;
	std::vector<ggp::Entity> m_entities;
	ggp::TransformHierarchy* m_transformHierarchy;

	size_t m_activeCamera;
	std::vector<std::shared_ptr<ggp::Camera>> m_cameras;

	std::shared_ptr<SimpleVertexShader> m_vertexShader;
	std::shared_ptr<SimplePixelShader> m_pixelShader;
	std::shared_ptr<SimplePixelShader> m_pixelShaderNormal;
	std::shared_ptr<SimplePixelShader> m_pixelShaderUV;
	std::shared_ptr<SimplePixelShader> m_pixelShaderCustom;
};

