#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "memutils.h"
#include "errors.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "BlockAllocator.h"

#include <DirectXMath.h>

#include <array>
#include <algorithm>

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;
using namespace ggp; // TODO: maybe just put Game in ggp namespace

// --------------------------------------------------------
// Called once per program, after the window and graphics API
// are initialized but before the game loop begins
// --------------------------------------------------------
void Game::Initialize()
{
	// alloc framerate history buffer and fill with zeroes
	m_framerateHistory = std::make_unique<decltype(m_framerateHistory)::element_type>();
	std::fill(m_framerateHistory->begin(), m_framerateHistory->end(), 0.0f);

	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	LoadShaders();
	CreateGeometry();
	m_transformHierarchy = Transform::CreateHierarchySingleton();
	CreateEntities();
	m_cameras.push_back(std::make_shared<Camera>(Camera::Options{
		.aspectRatio = Window::AspectRatio(),
		.initialGlobalPosition = { 0, 0, -1 },
		.initialRotation = { -DegToRad(45), DegToRad(90)},
		}));
	m_cameras.push_back(std::make_shared<Camera>(Camera::Options{
		.aspectRatio = Window::AspectRatio(),
		.fovDegrees = 70,
		.initialGlobalPosition = { -1, 0, -1 },
		}));
	m_activeCamera = 0;

	// Set initial graphics API state
	//  - These settings persist until we change them
	//  - Some of these, like the primitive topology & input layout, probably won't change
	//  - Others, like setting shaders, will need to be moved elsewhere later
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	{
		// create dynamic constant buffer on gpu
		const D3D11_BUFFER_DESC desc{
			.ByteWidth = alignsize<decltype(m_constantBufferCPUSide), 16>::value,
			.Usage = D3D11_USAGE_DYNAMIC,
			.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
			.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
		};
		Graphics::Device->CreateBuffer(&desc, nullptr, m_constantBuffer.GetAddressOf());

		// initial color. not mapped / put into gpu until draw
		m_constantBufferCPUSide.color = { 1.0f, 0.5f, 0.5f, 1.0f };

		// bind cb and keep binding for the whole program
		Graphics::Context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
	}

	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(Window::Handle());
	ImGui_ImplDX11_Init(Graphics::Device.Get(), Graphics::Context.Get());
	// Pick a style (uncomment one of these 3)
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();
	//ImGui::StyleColorsClassic();
}


// --------------------------------------------------------
// Clean up memory or objects created by this class
// 
// Note: Using smart pointers means there probably won't
//       be much to manually clean up here!
// --------------------------------------------------------
Game::~Game()
{
	Transform::DestroyHierarchySingleton(&m_transformHierarchy);
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


void Game::LoadShaders()
{
	m_vertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	m_pixelShader = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShader.cso").c_str());
}

void Game::CreateEntities()
{
	Mesh* tri = &m_alwaysLoadedMeshes["triangle"];
	Entity root(tri);
	root.GetTransform().SetPosition({ -0.5f, -0.5f, -0.0f });
	Entity out1(tri, root.GetTransform().AddChild());
	Entity out2(tri, root.GetTransform().AddChild());
	Entity out3(tri, root.GetTransform().AddChild());

	XMVECTOR pointOne = VectorSplat(.1f);
	out1.GetTransform().ScaleVec(pointOne);
	out2.GetTransform().ScaleVec(pointOne);
	out3.GetTransform().ScaleVec(pointOne);

	out1.GetTransform().MoveAbsoluteLocal({ 0.2f, 0.2f, 0.f });
	out2.GetTransform().MoveAbsoluteLocal({ -0.2f, 0.0f, 0.1f });
	out3.GetTransform().MoveAbsoluteLocal({ 0.0f, -0.3f, 0.05f });

	Mesh* drop = &m_alwaysLoadedMeshes["droplet"];
	Entity droplet1(drop, out1.GetTransform().AddChild());
	Entity droplet2(drop, out1.GetTransform().AddChild());
	Entity droplet3(drop, out1.GetTransform().AddChild());

	droplet1.GetTransform().MoveAbsoluteLocal({ 0.2f, 0.2f, 0.f });
	droplet2.GetTransform().MoveAbsoluteLocal({ -0.2f, 0.0f, 0.1f });
	droplet3.GetTransform().MoveAbsoluteLocal({ 0.0f, -0.3f, 0.05f });

	m_entities.push_back(root);
	m_entities.push_back(out1);
	m_entities.push_back(out2);
	m_entities.push_back(out3);
	m_entities.push_back(droplet1);
	m_entities.push_back(droplet2);
	m_entities.push_back(droplet3);
}

// --------------------------------------------------------
// Creates the geometry we're going to draw
// --------------------------------------------------------
void Game::CreateGeometry()
{
	constexpr XMFLOAT4 red = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
	constexpr XMFLOAT4 green = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
	constexpr XMFLOAT4 blue = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);

	Vertex triangle_vertices[] = {
		{ {+0.0f, +0.5f, +0.0f}, red },
		{ {+0.5f, -0.5f, +0.0f}, blue },
		{ {-0.5f, -0.5f, +0.0f}, green },
	};

	u32 triangle_indices[] = { 0, 1, 2 };

	constexpr float pi = 3.141592653f;
	constexpr float degtorad = pi / 180.f;

	Vertex hexagon_vertices[] = {
		{ {0.1f * cosf(degtorad * 30), 0.1f * sinf(degtorad * 30), 0}, red},
		{ {0.1f * cosf(degtorad * 90), 0.1f * sinf(degtorad * 90), 0}, blue},
		{ {0.1f * cosf(degtorad * 150), 0.1f * sinf(degtorad * 150), 0}, green},
		{ {0.1f * cosf(degtorad * 210), 0.1f * sinf(degtorad * 210), 0}, red},
		{ {0.1f * cosf(degtorad * 270), 0.1f * sinf(degtorad * 270), 0}, blue},
		{ {0.1f * cosf(degtorad * 330), 0.1f * sinf(degtorad * 330), 0}, green},
	};
	u32 hexagon_indices[] = { 5, 4, 0, 4, 3, 0, 3, 2, 0, 2, 1, 0 };

	Vertex droplet_vertices[] = {
		// top point
		{ {0, .15f, 0}, blue },
		// bottom circle
		{ { 0.1f * cosf(degtorad * 0),    0.1f * sinf(degtorad * 0), 0}, red },
		{ { 0.1f * cosf(degtorad * -45),  0.1f * sinf(degtorad * -45), 0}, red },
		{ { 0.1f * cosf(degtorad * -90),  0.1f * sinf(degtorad * -90), 0}, red },
		{ { 0.1f * cosf(degtorad * -135), 0.1f * sinf(degtorad * -135), 0}, red },
		{ { 0.1f * cosf(degtorad * -180), 0.1f * sinf(degtorad * -180), 0}, red },
	};
	u32 droplet_indices[] = { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5 };

	// upload all three meshes to the GPU
	// NOTE: funny default construction + overwrite happening here
	m_alwaysLoadedMeshes["triangle"] = Mesh(triangle_vertices, triangle_indices);
	m_alwaysLoadedMeshes["hexagon"] = Mesh(hexagon_vertices, hexagon_indices);
	m_alwaysLoadedMeshes["droplet"] = Mesh(droplet_vertices, droplet_indices);
}

void Game::UIBeginFrame(float deltaTime) noexcept
{
	// Feed fresh data to ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)Window::Width();
	io.DisplaySize.y = (float)Window::Height();
	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	// Determine new input capture
	Input::SetKeyboardCapture(io.WantCaptureKeyboard);
	Input::SetMouseCapture(io.WantCaptureMouse);
}

void Game::UIEndFrame() noexcept
{
	ImGui::Render(); // Turns this frame’s UI into renderable triangles
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // Draws it to the screen
}

// --------------------------------------------------------
// Handle resizing to match the new window size
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	if (!m_cameras.empty() && m_activeCamera < m_cameras.size())
		m_cameras[m_activeCamera]->UpdateProjectionMatrix(Window::AspectRatio(), Window::Width(), Window::Height());
}

static void SpinRecursive(float delta, float totalTime, Transform t, int depth = 0, int index = 0)
{
	if (auto c = t.GetFirstChild())
	{
		SpinRecursive(delta, totalTime, c.value(), depth + 1, 0);
	}
	if (auto s = t.GetNextSibling())
	{
		SpinRecursive(delta, totalTime, s.value(), depth, index + 1);
	}

	t.SetPosition({ .0f + (index / 3.f), .0f + (0.3f * sin(totalTime + depth + index)), 0.1f * (index + depth) });
	float scale = ((sinf(totalTime) + 1) * 0.25f) + 0.5f;
	t.SetScale({ scale, scale, 1.0f });
	t.Rotate({ 0.f, 0.f, delta });
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	// update fps sample graph
	{
		// push all the other samples down one
		const auto lastSampleIndex = m_framerateHistory->size() - 1;
		for (size_t i = 0; i < lastSampleIndex; ++i)
			(*m_framerateHistory)[i] = (*m_framerateHistory)[i + 1];
		// put current framerate on the end of the graph
		(*m_framerateHistory)[lastSampleIndex] = ImGui::GetIO().Framerate;
	}

	if (m_spinningEnabled)
	{
		Transform root = m_entities[0].GetTransform();
		SpinRecursive(deltaTime / 50.f, totalTime / 50.f, root);
	}

	gassert(m_activeCamera < m_cameras.size());
	m_cameras[m_activeCamera]->Update(deltaTime);

	// UI frame ended in Draw()
	UIBeginFrame(deltaTime);

	BuildUI();
}


void Game::BuildUI() noexcept
{
	ImGui::Begin("debug menu");

	ImGui::Text("Framerate: %f", ImGui::GetIO().Framerate);
	ImGui::Text("Window pixel dimensions: %d / %d", Window::Width(), Window::Height());
	ImGui::ColorEdit4("Background Color", m_backgroundColor.data(), 0);

	if (ImGui::Button("Show ImGui Demo Window", { 200, 50 }))
		m_demoWindowVisible = !m_demoWindowVisible;

	if (m_demoWindowVisible)
		ImGui::ShowDemoWindow();

	for (size_t i = 0; i < m_cameras.size(); ++i)
	{
		if (ImGui::RadioButton((std::string("Camera ") + std::to_string(i)).c_str(), m_activeCamera == i))
			m_activeCamera = i;

		Camera& cam = *m_cameras[i];
		ImGui::Text("FOV: %f", RadToDeg(cam.GetFov()));
		XMFLOAT3 pos = cam.GetTransform().GetPosition();
		ImGui::Text("Pos: %4.2f %4.2f %4.2f", pos.x, pos.y, pos.z);
	}

	// plot the fps samples
	ImGui::PlotHistogram("FPS Graph", m_framerateHistory->data(), static_cast<int>(m_framerateHistory->size()));

	std::array<char, 64> textBuffer = { 0 };
	if (ImGui::InputText("Text Entry", textBuffer.data(), textBuffer.size())) {
		m_nextStringUp = textBuffer.data();
	}
	if (ImGui::Button("Submit") && !m_nextStringUp.empty()) {
		m_lastTypedStrings.emplace_back(m_nextStringUp);
		m_nextStringUp.clear();
		// if it gets too big, erase one like a LIFO
		if (m_lastTypedStrings.size() > maxStrings)
			m_lastTypedStrings.erase(m_lastTypedStrings.begin());
	}

	// display pushed text
	for (const auto& str : m_lastTypedStrings) {
		ImGui::BulletText(str.c_str());
	}

	const char* last = m_lastTypedStrings.empty() ? "NO STRING ENTERED" : m_lastTypedStrings.back().c_str();
	if (ImGui::Button("Print the last entered word", { 200, 50 }))
		printf("last word: %s\n", last);

	ImGui::ColorEdit4("Color", &m_constantBufferCPUSide.color.x);

	if (ImGui::Checkbox("Enable spinning and stuff (prevents DragFloat3 from working, setting every frame)", &m_spinningEnabled))
	{
		std::array<char, 64> buf;
		for (size_t i = 0; i < m_entities.size(); ++i) {
			// NOTE: using snprintf here instead of std::string or BulletTextV because I was having issues with each and was tired
			// TODO: make this use BulletTextV and TreeNodeExV
			int bytes_printed = std::snprintf(buf.data(), buf.size(), "entity %zu", i);
			gassert(bytes_printed < buf.size());

			ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen;
			if (ImGui::TreeNodeEx(buf.data(), flag))
			{
				ImGui::PushID(i32(i));
				Entity& entity = m_entities[i];
				constexpr auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

				XMFLOAT3 pos = entity.GetTransform().GetPosition();
				if (ImGui::DragFloat3("Position", &pos.x, 0.01f, -1.f, 1.f)) {
					entity.GetTransform().SetPosition(pos);
				}

				pos = entity.GetTransform().GetEulerAngles();
				if (ImGui::DragFloat3("Rotation (Radians)", &pos.x, 0.01f, -1.f, 1.f)) {
					entity.GetTransform().SetEulerAngles(pos);
				}

				pos = entity.GetTransform().GetScale();
				if (ImGui::DragFloat3("Scale", &pos.x, 0.01f, -1.f, 1.f)) {
					entity.GetTransform().SetScale(pos);
				}

				bytes_printed = std::snprintf(buf.data(), buf.size(), "Vertices: %zu", entity.GetMesh()->GetVertexCount());
				gassert(bytes_printed < buf.size());
				ImGui::BulletText(buf.data());

				bytes_printed = std::snprintf(buf.data(), buf.size(), "Indices: %zu", entity.GetMesh()->GetIndexCount());
				gassert(bytes_printed < buf.size());
				ImGui::BulletText(buf.data());

				bytes_printed = std::snprintf(buf.data(), buf.size(), "Triangles: %zu", entity.GetMesh()->GetIndexCount() / 3);
				gassert(bytes_printed < buf.size());
				ImGui::BulletText(buf.data());

				ImGui::PopID();

				// close this treenode and move to next one
				ImGui::TreePop();
			}
		}
	}

	ImGui::End();
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Frame START
	// - These things should happen ONCE PER FRAME
	// - At the beginning of Game::Draw() before drawing *anything*
	{
		// Clear the back buffer (erase what's on screen) and depth buffer
		Graphics::Context->ClearRenderTargetView(Graphics::BackBufferRTV.Get(), m_backgroundColor.data());
		Graphics::Context->ClearDepthStencilView(Graphics::DepthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	for (ggp::Entity& entity : m_entities)
	{
		// send cb to shaders
		{
			gassert(m_activeCamera < m_cameras.size());
			Camera& camera = *m_cameras[m_activeCamera];
			D3D11_MAPPED_SUBRESOURCE cbMapped{};
			Graphics::Context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);

			((cb::WVPAndColor*)cbMapped.pData)->worldMatrix = *entity.GetTransform().GetWorldMatrixPtr();
			((cb::WVPAndColor*)cbMapped.pData)->viewMatrix = *camera.GetViewMatrix();
			((cb::WVPAndColor*)cbMapped.pData)->projectionMatrix = *camera.GetProjectionMatrix();
			((cb::WVPAndColor*)cbMapped.pData)->color = m_constantBufferCPUSide.color;

			// memcpy(cbMapped.pData, &m_constantBufferCPUSide, sizeof(cb::TransformAndColor));

			Graphics::Context->Unmap(m_constantBuffer.Get(), 0);
		}
		entity.GetMesh()->BindBuffersAndDraw();
	}

	// begun in Update()
	UIEndFrame();

	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Present at the end of the frame
		bool vsync = Graphics::VsyncState();
		Graphics::SwapChain->Present(
			vsync ? 1 : 0,
			vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Re-bind back buffer and depth buffer after presenting
		Graphics::Context->OMSetRenderTargets(
			1,
			Graphics::BackBufferRTV.GetAddressOf(),
			Graphics::DepthBufferDSV.Get());
	}
}



