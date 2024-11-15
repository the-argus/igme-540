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

#include <DirectXMath.h>
#include <WICTextureLoader.h>

#include <algorithm>

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

using namespace DirectX;

// externs declared in Material.h
ID3D11SamplerState* ggp::defaultSamplerState;
ID3D11ShaderResourceView* ggp::defaultNormalTextureView;
ID3D11ShaderResourceView* ggp::defaultAlbedoTextureView;
ID3D11ShaderResourceView* ggp::defaultSpecularTextureView;

// recursively position entites around each other
static void PositionEntities(ggp::Transform t, u32 siblingCount = 1, u32 depth = 0, u32 length = 0)
{
	using namespace ggp;
	if (auto c = t.GetFirstChild())
	{
		PositionEntities(c.value(), t.GetChildCount(), depth + 1, 0);
	}

	if (auto s = t.GetNextSibling())
	{
		gassert(siblingCount > 1, "there were not supposed to be siblings...");
		PositionEntities(s.value(), siblingCount, depth, length + 1);
	}

	XMFLOAT3 localPosition{};
	// position the object around the Y axis
	XMScalarSinCosEst(&localPosition.z, &localPosition.x, XM_2PI * (length / siblingCount));

	// every other set of children is positioned around X axis instead
	//if (depth % 2)
	//	std::swap(localPosition.z, localPosition.y);

	// multiply distance by one times length + 1
	XMVECTOR loaded = XMLoadFloat3(&localPosition);
	// loaded = XMVectorMultiply(loaded, VectorSplat(length + 1));
	loaded = XMVectorAdd(loaded, VectorSplat(2));

	t.StoreLocalPosition(loaded);
}

static void SpinRecursive(float delta, ggp::Transform t)
{
	if (auto c = t.GetFirstChild())
	{
		SpinRecursive(delta, c.value());
	}
	if (auto s = t.GetNextSibling())
	{
		SpinRecursive(delta, s.value());
	}

	t.Rotate({ 0.f, 0.f, delta / 10.f });
}

static void LoadTexture(ID3D11ShaderResourceView** out_srv, const char* texturename, std::wstring assetsSubDir = L"example_textures/ ")
{
	using namespace ggp;
	const auto wideName = std::wstring(texturename, texturename + std::strlen(texturename));
	const auto path = std::wstring(L"../../assets/") + assetsSubDir + wideName + std::wstring(L".png");
	const auto result = CreateWICTextureFromFile(
		Graphics::Device.Get(),
		Graphics::Context.Get(),
		FixPath(path).c_str(),
		nullptr, out_srv);
	gassert(result == S_OK);
}

void ggp::Game::Initialize()
{
	// alloc framerate history buffer and fill with zeroes
	m_framerateHistory = std::make_unique<decltype(m_framerateHistory)::element_type>();
	std::fill(m_framerateHistory->begin(), m_framerateHistory->end(), 0.0f);

	LoadShaders();

	// load textures
	{
		// default textures
		constexpr auto defaultTextureDir = L"example_textures/fallback/";
		LoadTexture(&defaultAlbedoTextureView, "missing_albedo", defaultTextureDir);
		LoadTexture(&defaultNormalTextureView, "flat_normals", defaultTextureDir);
		LoadTexture(&defaultSpecularTextureView, "no_specular", defaultTextureDir);

		constexpr std::array exampleSpecularTextures{
			"brokentiles",
			"brokentiles_specular",
			"rustymetal",
			"rustymetal_specular",
			"tiles",
			"tiles_specular",
		};

		for (const auto& exampleTexName : exampleSpecularTextures) {
			com_p<ID3D11ShaderResourceView> srv;
			LoadTexture(srv.GetAddressOf(), exampleTexName, L"example_textures/specular_examples/");
			m_textureViews.insert({ exampleTexName, srv });
		}
	}

	// create default sampler
	{
		constexpr D3D11_SAMPLER_DESC samplerDescription{
			.Filter = D3D11_FILTER_ANISOTROPIC,
			.AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
			.AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
			.AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
			.MaxAnisotropy = 4,
			.MaxLOD = D3D11_FLOAT32_MAX,
		};

		const auto createSamplerRes = Graphics::Device->CreateSamplerState(&samplerDescription, m_defaultSampler.GetAddressOf());
		gassert(createSamplerRes == S_OK);
		gassert(m_defaultSampler);
	}

	// create global default (fallback) sampler
	{
		constexpr D3D11_SAMPLER_DESC samplerDescription{
			.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
			.AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
			.AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
			.AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
			.MaxLOD = D3D11_FLOAT32_MAX,
		};
		const auto createSamplerRes = Graphics::Device->CreateSamplerState(&samplerDescription, &defaultSamplerState);
		gassert(createSamplerRes == S_OK);
	}

	// group shaders and colors into materials
	m_materials["base"] = std::make_unique<Material>(m_vertexShader.get(), m_pixelShader.get(), Material::Options{
		.colorRGBA = {0.5f, 0.5f, 1.f, 1.f },
		.roughness = 0.5f,
		});
	m_materials["brokentiles"] = std::make_unique<Material>(m_vertexShader.get(), m_pixelShader.get(), Material::Options{
		.colorRGBA = {0.5f, 0.5f, 1.f, 1.f },
		.roughness = 0.4f,
		.samplerState = m_defaultSampler.Get(),
		.albedoTextureView = m_textureViews.at("brokentiles").Get(),
		.specularTextureView = m_textureViews.at("brokentiles_specular").Get(),
		});
	m_materials["tiles"] = std::make_unique<Material>(m_vertexShader.get(), m_pixelShader.get(), Material::Options{
		.colorRGBA = {0.5f, 0.5f, 1.f, 1.f },
		.roughness = 0.5f,
		.uvOffset = { 0.5f, 0.5f },
		.samplerState = m_defaultSampler.Get(),
		.albedoTextureView = m_textureViews.at("tiles").Get(),
		.specularTextureView = m_textureViews.at("tiles_specular").Get(),
		});
	m_materials["rustymetal"] = std::make_unique<Material>(m_vertexShader.get(), m_pixelShader.get(), Material::Options{
		.colorRGBA = {0.5f, 0.5f, 1.f, 1.f },
		.roughness = 0.3f,
		.samplerState = m_defaultSampler.Get(),
		.albedoTextureView = m_textureViews.at("rustymetal").Get(),
		.specularTextureView = m_textureViews.at("rustymetal_specular").Get(),
		});

	m_lights = {
		Light{
			.type = LIGHT_TYPE_DIRECTIONAL,
			.direction = { 0.f, 0.f, 1.f },
			.range = {},
			.position = {},
			.intensity = 1.f,
			.color = { 1.f, 0.f, 0.f },
		},
		Light{
			.type = LIGHT_TYPE_DIRECTIONAL,
			.direction = { 0.f, 0.f, -1.f },
			.range = {},
			.position = {},
			.intensity = 1.f,
			.color = { 0.f, 0.f, 1.f },
		},
		Light{
			.type = LIGHT_TYPE_DIRECTIONAL,
			.direction = { 0.f, 1.f, 0.f },
			.range = {},
			.position = {},
			.intensity = 1.f,
			.color = { 0.f, 1.f, 0.f },
		},
		Light{
			.type = LIGHT_TYPE_POINT,
			.range = 30.f,
			.position = { 1.f, 1.f, 1.f },
			.intensity = 0.3f,
			.color = { 1.f, 1.f, 1.f },
		},
		Light{
			.type = LIGHT_TYPE_POINT,
			.range = 30.f,
			.position = { 2.f, -1.f, 1.f },
			.intensity = 0.3f,
			.color = { 1.f, 1.f, 1.f },
		},
	};

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

	Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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
ggp::Game::~Game()
{
	Transform::DestroyHierarchySingleton(&m_transformHierarchy);
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


void ggp::Game::LoadShaders()
{
	m_vertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"forward_vs_base.cso").c_str());
	m_pixelShader = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"forward_ps_pbr.cso").c_str());
}

void ggp::Game::CreateEntities()
{
	Mesh* cube = &m_alwaysLoadedMeshes.at("cube.obj");
	Mesh* cylinder = &m_alwaysLoadedMeshes.at("cylinder.obj");
	Mesh* helix = &m_alwaysLoadedMeshes.at("helix.obj");
	Mesh* quad = &m_alwaysLoadedMeshes.at("quad.obj");
	Mesh* sphere = &m_alwaysLoadedMeshes.at("sphere.obj");
	Mesh* quad_double_sided = &m_alwaysLoadedMeshes.at("quad_double_sided.obj");
	Mesh* torus = &m_alwaysLoadedMeshes.at("torus.obj");

	Entity root(cube, m_materials.at("phong").get());

	Entity layer00(cube, m_materials.at("rustymetal").get(), root.GetTransform().AddChild());
	Entity layer01(cube, m_materials.at("tiles").get(), root.GetTransform().AddChild());

	Entity out1(helix, m_materials.at("phong").get(), layer00.GetTransform().AddChild());
	Entity out2(quad, m_materials.at("tiles").get(), layer00.GetTransform().AddChild());
	Entity out3(sphere, m_materials.at("rustymetal").get(), layer00.GetTransform().AddChild());

	Entity droplet1(quad_double_sided, m_materials.at("rustymetal").get(), out1.GetTransform().AddChild());
	Entity droplet2(torus, m_materials.at("tiles").get(), out1.GetTransform().AddChild());
	Entity droplet3(cube, m_materials.at("rustymetal").get(), out1.GetTransform().AddChild());

	// recursively position all the entites around each other
	// PositionEntities(root.GetTransform());

	m_entities.push_back(root);
	m_entities.push_back(layer00);
	m_entities.push_back(layer01);
	m_entities.push_back(out1);
	m_entities.push_back(out2);
	m_entities.push_back(out3);
	m_entities.push_back(droplet1);
	m_entities.push_back(droplet2);
	m_entities.push_back(droplet3);

	for (int i = 0; i < m_entities.size(); ++i) {
		f32 yOffset = m_entities[i].GetTransform().GetParent() ? -1.f : 0.f;
		m_entities[i].GetTransform().SetLocalPosition({ f32(i) * 2.f, yOffset, 0.f });
	}
}

void ggp::Game::CreateGeometry()
{
	constexpr std::array files = {
		"cube.obj",
		"cylinder.obj",
		"helix.obj",
		"quad.obj",
		"sphere.obj",
		"quad_double_sided.obj",
		"torus.obj",
	};

	for (auto* filename : files) {
		const auto wideFilename = std::wstring(filename, filename + std::strlen(filename));
		const auto truePath = FixPath(std::wstring(L"../../assets/example_meshes/") + wideFilename);
		auto [verts, indices] = Mesh::ReadOBJ(truePath.c_str());
		m_alwaysLoadedMeshes[filename] = Mesh::UploadToGPU(verts, indices);
	}
}

void ggp::Game::UIBeginFrame(float deltaTime) noexcept
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

void ggp::Game::UIEndFrame() noexcept
{
	ImGui::Render(); // Turns this frame�s UI into renderable triangles
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // Draws it to the screen
}

void ggp::Game::OnResize()
{
	if (!m_cameras.empty() && m_activeCamera < m_cameras.size())
		m_cameras[m_activeCamera]->UpdateProjectionMatrix(Window::AspectRatio(), Window::Width(), Window::Height());
}

void ggp::Game::Update(float deltaTime, float totalTime)
{
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
		SpinRecursive(deltaTime, root);
	}

	gassert(m_activeCamera < m_cameras.size());
	m_cameras[m_activeCamera]->Update(deltaTime);

	// UI frame ended in Draw()
	UIBeginFrame(deltaTime);

	BuildUI();
}


void ggp::Game::BuildUI() noexcept
{
	ImGui::Begin("debug menu");

	ImGui::Text("Press F to move camera");

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

	for (size_t i = 0; i < m_lights.size(); ++i) {
		std::array<char, 64> buf;
		int bytes_printed = std::snprintf(buf.data(), buf.size(), "light %zu", i);
		gassert(bytes_printed < buf.size());
		ImGui::ColorEdit3(buf.data(), &m_lights[i].color.x);
	}

	ImGui::ColorEdit3("Ambient Color", &m_ambientColor.x);

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

void ggp::Game::Draw(float deltaTime, float totalTime)
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
		// activate entity's shaders
		entity.GetMaterial()->GetPixelShader()->SetShader();
		entity.GetMaterial()->GetVertexShader()->SetShader();

		// send cb to shaders
		{
			gassert(m_activeCamera < m_cameras.size());
			Camera& camera = *m_cameras[m_activeCamera];

			auto* vs = entity.GetMaterial()->GetVertexShader();
			auto* ps = entity.GetMaterial()->GetPixelShader();

			vs->SetMatrix4x4("world", *entity.GetTransform().GetWorldMatrixPtr());
			vs->SetMatrix4x4("view", *camera.GetViewMatrix());
			vs->SetMatrix4x4("projection", *camera.GetProjectionMatrix());
			vs->SetMatrix4x4("worldInverseTranspose", *entity.GetTransform().GetWorldInverseTransposeMatrixPtr());

			ps->SetFloat4("colorTint", entity.GetMaterial()->GetColor());
			ps->SetFloat("roughness", entity.GetMaterial()->GetRoughness());
			ps->SetFloat4("ambient", m_ambientColor);
			ps->SetFloat3("cameraPosition", camera.GetTransform().GetPosition());
			ps->SetFloat("totalTime", totalTime);
			ps->SetData("lights", m_lights.data(), u32(m_lights.size() * sizeof(Light)));
			ps->SetFloat2("uvOffset", entity.GetMaterial()->GetUVOffset());
			ps->SetFloat2("uvScale", entity.GetMaterial()->GetUVScale());

			entity.GetMaterial()->BindSRVsAndSamplerStates();

			vs->CopyAllBufferData();
			ps->CopyAllBufferData();
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



