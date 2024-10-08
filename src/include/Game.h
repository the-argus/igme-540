#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

#include "Mesh.h"
#include "Entity.h"
#include "Camera.h"
#include "ConstantBuffers.h"
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

	bool m_spinningEnabled = true;
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

	// meshes that load at the start of the game and unload at the end- avoid lifetime management (for now)
	std::unordered_map<std::string, ggp::Mesh> m_alwaysLoadedMeshes;
	std::vector<ggp::Entity> m_entities;
	ggp::TransformHierarchy* m_transformHierarchy;

	size_t m_activeCamera;
	std::vector<std::shared_ptr<ggp::Camera>> m_cameras;

	ggp::com_p<ID3D11Buffer> m_constantBuffer;
	ggp::cb::WVPAndColor m_constantBufferCPUSide;

	// Note the usage of ComPtr below
	//  - This is a smart pointer for objects that abide by the
	//     Component Object Model, which DirectX objects do
	//  - More info here: https://github.com/Microsoft/DirectXTK/wiki/ComPtr

	// Shaders and shader-related constructs
	ggp::com_p<ID3D11PixelShader> pixelShader;
	ggp::com_p<ID3D11VertexShader> vertexShader;
	ggp::com_p<ID3D11InputLayout> inputLayout;
};

