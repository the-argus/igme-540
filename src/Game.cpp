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
namespace ggp
{
	com_p<ID3D11SamplerState> defaultSamplerState = {};
	com_p<ID3D11ShaderResourceView> defaultNormalTextureView = {};
	com_p<ID3D11ShaderResourceView> defaultAlbedoTextureView = {};
	com_p<ID3D11ShaderResourceView> defaultMetalnessTextureViewMetal = {};
	com_p<ID3D11ShaderResourceView> defaultMetalnessTextureViewNonMetal = {};
	SimpleVertexShader* defaultVertexShader = {};
	SimplePixelShader* defaultPixelShader = {};
}

static const std::array lights = {
	ggp::Light{
		.type = LIGHT_TYPE_DIRECTIONAL,
		.direction = { 0.f, -1.f, -1.f },
		.intensity = 1.f,
		.color = { 1.f, 1.f, 1.f },
	},
	ggp::Light{
		.type = LIGHT_TYPE_DIRECTIONAL,
		.direction = { 0.f, 0.f, -1.f },
		.intensity = 0.0f,
		.color = { 0.f, 0.f, 1.f },
	},
	ggp::Light{
		.type = LIGHT_TYPE_DIRECTIONAL,
		.direction = { 0.f, 1.f, 0.f },
		.intensity = 0.0f,
		.color = { 0.f, 1.f, 0.f },
	},
	ggp::Light{
		.type = LIGHT_TYPE_POINT,
		.range = 100.f,
		.position = { 1.f, 3.f, 1.f },
		.intensity = 0.7f,
		.color = { 1.f, 1.f, 1.f },
	},
	ggp::Light{
		.type = LIGHT_TYPE_POINT,
		.range = 30.f,
		.position = { 2.f, -1.f, 1.f },
		.intensity = 0.3f,
		.color = { 1.f, 1.f, 1.f },
	},
};

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
	LoadShaders();
	LoadTextures();
	CreateSamplers();
	CreateMaterials();
	LoadMeshes();
	LoadCubemapAndCreateSkybox();
	// copy in initial state of lights from static variable
	m_lights = std::make_unique<decltype(m_lights)::element_type>(lights);
	// initialize transform hierarchy singleton, we have a member variable reference to it
	m_transformHierarchy = Transform::CreateHierarchySingleton();
	CreateEntities();

	m_cameras.push_back(std::make_shared<Camera>(Camera::Options{
		.aspectRatio = Window::AspectRatio(),
		.initialGlobalPosition = { 5.3f, 0.5f, -3.3f },
		.initialRotation = { 0, DegToRad(180) },
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

ggp::Game::~Game()
{
	Transform::DestroyHierarchySingleton(&m_transformHierarchy);
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	// non owning pointers which will be dangling references if not set to 0
	defaultPixelShader = {};
	defaultVertexShader = {};
	// owning pointers which have global lifetime so they need to be explicitly destroyed to unload textures and samplers
	defaultAlbedoTextureView = nullptr;
	defaultMetalnessTextureViewMetal = nullptr;
	defaultMetalnessTextureViewNonMetal = nullptr;
	defaultNormalTextureView = nullptr;
	defaultSamplerState = nullptr;
}

void ggp::Game::LoadTextures()
{
	// globally accessible fallback textures
	constexpr auto defaultTextureDir = L"example_textures/fallback/";
	LoadTexture(defaultAlbedoTextureView.GetAddressOf(), "missing_albedo", defaultTextureDir);
	LoadTexture(defaultNormalTextureView.GetAddressOf(), "flat_normals", defaultTextureDir);
	LoadTexture(defaultMetalnessTextureViewMetal.GetAddressOf(), "metal", defaultTextureDir);
	LoadTexture(defaultMetalnessTextureViewNonMetal.GetAddressOf(), "non_metal", defaultTextureDir);

	constexpr std::array textures{
		"bronze_albedo",
		"bronze_metal",
		"bronze_normals",
		"bronze_roughness",
		"cobblestone_albedo",
		"cobblestone_metal",
		"cobblestone_normals",
		"cobblestone_roughness",
		"floor_albedo",
		"floor_metal",
		"floor_normals",
		"floor_roughness",
		"paint_albedo",
		"paint_metal",
		"paint_normals",
		"paint_roughness",
		"rough_albedo",
		"rough_metal",
		"rough_normals",
		"rough_roughness",
		"scratched_albedo",
		"scratched_metal",
		"scratched_normals",
		"scratched_roughness",
		"wood_albedo",
		"wood_metal",
		"wood_normals",
		"wood_roughness",
	};

	for (const auto& name : textures) {
		com_p<ID3D11ShaderResourceView> srv;
		LoadTexture(srv.GetAddressOf(), name, L"materials/");
		m_textureViews.insert({ name, srv });
	}
}

void ggp::Game::CreateSamplers()
{
	// default sampler, anisotropic
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

	// create global default (fallback) sampler
	constexpr D3D11_SAMPLER_DESC fallbackSamplerDescription{
		.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
		.AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
		.MaxLOD = D3D11_FLOAT32_MAX,
	};
	const auto fallbackSamplerRes = Graphics::Device->CreateSamplerState(&fallbackSamplerDescription, defaultSamplerState.GetAddressOf());
	gassert(fallbackSamplerRes == S_OK);
}

void ggp::Game::LoadShaders()
{
	m_vertexShader = std::make_unique<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"forward_vs_base.cso").c_str());
	m_pixelShader = std::make_unique<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"forward_ps_pbr.cso").c_str());
	defaultPixelShader = m_pixelShader.get();
	defaultVertexShader = m_vertexShader.get();
}

void ggp::Game::CreateMaterials()
{
	m_materials["missing"] = std::make_unique<Material>(Material::Options{
		.roughness = 0.5f,
		});
	m_materials["flat_red_wood"] = std::make_unique<Material>(Material::Options{
		.colorRGBA = {1.f, 0.5f, 0.5f, 1.f },
		.roughness = 0.f,
		.albedoTextureView = m_textureViews.at("wood_albedo").Get(),
		});
	m_materials["bronze"] = std::make_unique<Material>(Material::Options{
		.albedoTextureView = m_textureViews.at("bronze_albedo").Get(),
		.normalTextureView = m_textureViews.at("bronze_normals").Get(),
		.roughnessTextureView = m_textureViews.at("bronze_roughness").Get(),
		.metalnessTextureView = m_textureViews.at("bronze_metal").Get(),
		});
	m_materials["cobblestone"] = std::make_unique<Material>(Material::Options{
		.albedoTextureView = m_textureViews.at("cobblestone_albedo").Get(),
		.normalTextureView = m_textureViews.at("cobblestone_normals").Get(),
		.roughnessTextureView = m_textureViews.at("cobblestone_roughness").Get(),
		.metalnessTextureView = m_textureViews.at("cobblestone_metal").Get(),
		});
	m_materials["floor"] = std::make_unique<Material>(Material::Options{
		.albedoTextureView = m_textureViews.at("floor_albedo").Get(),
		.normalTextureView = m_textureViews.at("floor_normals").Get(),
		.roughnessTextureView = m_textureViews.at("floor_roughness").Get(),
		.metalnessTextureView = m_textureViews.at("floor_metal").Get(),
		});
	m_materials["paint"] = std::make_unique<Material>(Material::Options{
		.albedoTextureView = m_textureViews.at("paint_albedo").Get(),
		.normalTextureView = m_textureViews.at("paint_normals").Get(),
		.roughnessTextureView = m_textureViews.at("paint_roughness").Get(),
		.metalnessTextureView = m_textureViews.at("paint_metal").Get(),
		});
	m_materials["rough"] = std::make_unique<Material>(Material::Options{
		.albedoTextureView = m_textureViews.at("rough_albedo").Get(),
		.normalTextureView = m_textureViews.at("rough_normals").Get(),
		.roughnessTextureView = m_textureViews.at("rough_roughness").Get(),
		.metalnessTextureView = m_textureViews.at("rough_metal").Get(),
		});
	m_materials["scratched"] = std::make_unique<Material>(Material::Options{
		.albedoTextureView = m_textureViews.at("scratched_albedo").Get(),
		.normalTextureView = m_textureViews.at("scratched_normals").Get(),
		.roughnessTextureView = m_textureViews.at("scratched_roughness").Get(),
		.metalnessTextureView = m_textureViews.at("scratched_metal").Get(),
		});
	m_materials["wood"] = std::make_unique<Material>(Material::Options{
		.albedoTextureView = m_textureViews.at("wood_albedo").Get(),
		.normalTextureView = m_textureViews.at("wood_normals").Get(),
		.roughnessTextureView = m_textureViews.at("wood_roughness").Get(),
		.metalnessTextureView = m_textureViews.at("wood_metal").Get(),
		});
}

void ggp::Game::LoadMeshes()
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
		m_meshes[filename] = std::make_unique<Mesh>(std::move(Mesh::UploadToGPU(verts, indices)));
	}
}

void ggp::Game::LoadCubemapAndCreateSkybox()
{
	// skybox resources is an owning object which we keep for the duration of the game that we want to have skyboxes (the whole game probs)
	m_skyboxResources = Sky::SharedResources{
		.depthStencilState = Sky::CreateDepthStencilStateThatKeepsDeepPixels(),
		.rasterizerState = Sky::CreateRasterizerStateThatDrawsBackfaces(),
		.skyMesh = m_meshes.at("cube.obj").get(),
		.skyboxPixelShader = std::make_unique<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"skybox_ps.cso").c_str()),
		.skyboxVertexShader = std::make_unique<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"skybox_vs.cso").c_str()),
	};

	m_textureViews["pinkCloudsSkybox"] = Sky::LoadCubemap(Sky::LoadCubemapOptions{
		.right = FixPath(L"../../assets/example_textures/skyboxes/pink_clouds/right.png").c_str(),
		.left = FixPath(L"../../assets/example_textures/skyboxes/pink_clouds/left.png").c_str(),
		.up = FixPath(L"../../assets/example_textures/skyboxes/pink_clouds/up.png").c_str(),
		.down = FixPath(L"../../assets/example_textures/skyboxes/pink_clouds/down.png").c_str(),
		.front = FixPath(L"../../assets/example_textures/skyboxes/pink_clouds/front.png").c_str(),
		.back = FixPath(L"../../assets/example_textures/skyboxes/pink_clouds/back.png").c_str(),
		});

	m_skybox = std::make_unique<Sky>(m_textureViews.at("pinkCloudsSkybox").Get(), m_defaultSampler.Get());
}

void ggp::Game::CreateEntities()
{
	Mesh* cube = m_meshes.at("cube.obj").get();
	Mesh* cylinder = m_meshes.at("cylinder.obj").get();
	Mesh* helix = m_meshes.at("helix.obj").get();
	Mesh* quad = m_meshes.at("quad.obj").get();
	Mesh* sphere = m_meshes.at("sphere.obj").get();
	Mesh* quad_double_sided = m_meshes.at("quad_double_sided.obj").get();
	Mesh* torus = m_meshes.at("torus.obj").get();

	Entity root(cube, m_materials.at("bronze").get(), "bronze cube");

	Entity layer00(cube, m_materials.at("floor").get(), root.GetTransform().AddChild(), "floor cube");
	Entity layer01(cube, m_materials.at("scratched").get(), root.GetTransform().AddChild(), "scratched cube");

	Entity out1(helix, m_materials.at("cobblestone").get(), layer00.GetTransform().AddChild(), "cobblestone helix");
	Entity out2(quad, m_materials.at("wood").get(), layer00.GetTransform().AddChild(), "wood quad");
	Entity out3(sphere, m_materials.at("rough").get(), layer00.GetTransform().AddChild(), "rough sphere");

	Entity droplet1(quad_double_sided, m_materials.at("paint").get(), out1.GetTransform().AddChild(), "paint double sided quad");
	Entity droplet2(torus, m_materials.at("bronze").get(), out1.GetTransform().AddChild(), "bronze torus");
	Entity droplet3(cube, m_materials.at("cobblestone").get(), out1.GetTransform().AddChild(), "cobblestone cube");

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
	ImGui::Render(); // Turns this frame’s UI into renderable triangles
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

	ImGui::Text("Press F to toggle camera lock");
	ImGui::Text("CURRENT CAMERA LOCKED: %s", m_cameras[m_activeCamera]->IsLocked() ? "TRUE" : "FALSE");

	ImGui::Text("Framerate: %f", ImGui::GetIO().Framerate);
	ImGui::Text("Window pixel dimensions: %d / %d", Window::Width(), Window::Height());
	ImGui::ColorEdit4("Background Color", m_backgroundColor.data(), 0);

	for (size_t i = 0; i < m_cameras.size(); ++i)
	{
		if (ImGui::RadioButton((std::string("Camera ") + std::to_string(i)).c_str(), m_activeCamera == i))
			m_activeCamera = i;

		Camera& cam = *m_cameras[i];
		ImGui::Text("FOV: %f", RadToDeg(cam.GetFov()));
		XMFLOAT3 pos = cam.GetTransform().GetPosition();
		ImGui::Text("Pos: %4.2f %4.2f %4.2f", pos.x, pos.y, pos.z);
		ImGui::Text("Locked: %s", cam.IsLocked() ? "True" : "False");
	}

	for (size_t i = 0; i < m_lights->size(); ++i) {
		std::array<char, 64> buf;
		int bytes_printed = std::snprintf(buf.data(), buf.size(), "light %zu", i);
		gassert(bytes_printed < buf.size());
		ImGui::ColorEdit3(buf.data(), &(*m_lights)[i].color.x);
	}

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
	// Clear the back buffer (erase what's on screen) and depth buffer
	Graphics::Context->ClearRenderTargetView(Graphics::BackBufferRTV.Get(), m_backgroundColor.data());
	Graphics::Context->ClearDepthStencilView(Graphics::DepthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	gassert(m_activeCamera < m_cameras.size());
	Camera& camera = *m_cameras[m_activeCamera];
	for (Entity& entity : m_entities)
	{
		// activate entity's shaders
		entity.GetMaterial()->GetPixelShader()->SetShader();
		entity.GetMaterial()->GetVertexShader()->SetShader();

		// send cb to shaders
		{
			auto* vs = entity.GetMaterial()->GetVertexShader();
			auto* ps = entity.GetMaterial()->GetPixelShader();

			vs->SetMatrix4x4("world", *entity.GetTransform().GetWorldMatrixPtr());
			vs->SetMatrix4x4("view", *camera.GetViewMatrix());
			vs->SetMatrix4x4("projection", *camera.GetProjectionMatrix());
			vs->SetMatrix4x4("worldInverseTranspose", *entity.GetTransform().GetWorldInverseTransposeMatrixPtr());

			ps->SetFloat4("colorTint", entity.GetMaterial()->GetColor());
			ps->SetFloat("roughness", entity.GetMaterial()->GetRoughness());
			ps->SetFloat3("cameraPosition", camera.GetTransform().GetPosition());
			ps->SetFloat("totalTime", totalTime);
			ps->SetData("lights", m_lights->data(), u32(m_lights->size() * sizeof(Light)));
			ps->SetFloat2("uvOffset", entity.GetMaterial()->GetUVOffset());
			ps->SetFloat2("uvScale", entity.GetMaterial()->GetUVScale());

			entity.GetMaterial()->BindTextureViewsAndSamplerStates(Material::ShaderVariableNames{
				.sampler = "textureSampler",
				.albedoTexture = "albedoTexture",
				.normalTexture = "normalTexture",
				.roughnessTexture = "roughnessTexture",
				.metalnessTexture = "metalnessTexture",
				.roughnessEnabledInt = "useFlatRoughness",
				.roughness = "roughness",
				});

			vs->CopyAllBufferData();
			ps->CopyAllBufferData();
		}
		entity.GetMesh()->BindBuffersAndDraw();
	}

	m_skybox->Draw(m_skyboxResources, *camera.GetViewMatrix(), *camera.GetProjectionMatrix());

	// begun in Update()
	UIEndFrame();

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



