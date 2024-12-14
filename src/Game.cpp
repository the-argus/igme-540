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
		.isShadowCaster = true,
	},
	ggp::Light{
		.type = LIGHT_TYPE_POINT,
		.range = 100.f,
		.position = { -20.f, 3.f, 1.f },
		.intensity = 0.3f,
		.color = { 1.f, 1.f, 1.f },
	},
	ggp::Light{
		.type = LIGHT_TYPE_POINT,
		.range = 100.f,
		.position = { 20.f, 3.f, 1.f },
		.intensity = 0.3f,
		.color = { 1.f, 1.f, 1.f },
	},
	ggp::Light{
		.type = LIGHT_TYPE_POINT,
		.range = 100.f,
		.position = { 1.f, 3.f, -5.f },
		.intensity = 0.3f,
		.color = { 1.f, 1.f, 1.f },
	},
	ggp::Light{
		.type = LIGHT_TYPE_POINT,
		.range = 100.f,
		.position = { 1.f, 3.f, 5.f },
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

	t.SetLocalPosition({3 * f32(depth), f32(t.GetLocalPosition().y), 3 * f32(length)});
}

static void SpinRecursive(float delta, float totalTime, ggp::Transform t, int numSiblings = 1, int depth = 0, int length = 0)
{
	if (auto c = t.GetFirstChild())
	{
		SpinRecursive(delta, totalTime, c.value(), t.GetChildCount(), depth + 1, 0);
	}
	if (auto s = t.GetNextSibling())
	{
		SpinRecursive(delta, totalTime, s.value(), numSiblings, depth, length + 1);
	}

	const auto rotation = delta / (10);

	if (t.GetParent() && length == 0) {
		float scale = std::fmaxf(std::fabsf(std::cosf(totalTime + depth)), 0.1f);
		// TODO: support non-uniform rotations
		t.SetScale({ scale, scale, scale });
	}

	if (length == numSiblings - 1) {
		if (depth % 2 == 0) {
			t.RotateLocal({ 0.f, rotation, 0.f });
		} else {
			t.RotateLocal({ rotation, 0.f, 0.f });
		}
	}

	if (t.GetParent())
		t.RotateLocal({ 0.f, rotation * 5, 0.f });
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

	// copy in initial state of lights from static variable
	m_lights = std::make_unique<decltype(m_lights)::element_type>(lights);
	m_shadowMapResources = std::make_unique<decltype(m_shadowMapResources)::element_type>();

	CreateShadowMaps();
	CreateSamplers();
	CreateRenderTarget();
	CreateMaterials();
	LoadMeshes();
	LoadCubemapAndCreateSkybox();

	{
		constexpr D3D11_RASTERIZER_DESC shadowMapRasterizerStateDesc = {
			.FillMode = D3D11_FILL_SOLID,
			.CullMode = D3D11_CULL_BACK,
			.DepthBias = 1000,
			.SlopeScaledDepthBias = 1.0f,
			.DepthClipEnable = true,
		};

		auto result = Graphics::Device->CreateRasterizerState(
			&shadowMapRasterizerStateDesc, m_shadowMapRasterizerState.GetAddressOf());
		gassert(result == S_OK);
	}

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

void ggp::Game::CreateShadowMaps()
{
	for (size_t i = 0; i < m_lights->size(); ++i) {
		Light& light = (*m_lights)[i];
		if (!light.isShadowCaster)
			continue;
		gassert(light.type == LIGHT_TYPE_DIRECTIONAL, "only directional lights support shadows rn");

		const XMVECTOR lightDirection = XMLoadFloat3(&light.direction);
		const XMMATRIX lightView = XMMatrixLookToLH(
			XMVectorScale(lightDirection, -20), // Position: "Backing up" 20 units from origin
			lightDirection, // Direction: light's direction
			XMVectorSet(0, 1, 0, 0));
		XMStoreFloat4x4(&light.shadowView, lightView);

		const f32 lightProjectionSize = 70.0f;
		const XMMATRIX lightProjection = XMMatrixOrthographicLH(
			lightProjectionSize,
			lightProjectionSize,
			1.0f,
			100.0f);
		XMStoreFloat4x4(&light.shadowProjection, lightProjection);

		constexpr D3D11_TEXTURE2D_DESC shadowDesc = {
			.Width = shadowMapResolution,
			.Height = shadowMapResolution,
			.MipLevels = 1,
			.ArraySize = 1,
			.Format = DXGI_FORMAT_R32_TYPELESS,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0,
			},
			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
			.CPUAccessFlags = 0,
			.MiscFlags = 0,
		};

		com_p<ID3D11Texture2D> shadowTexture;
		Graphics::Device->CreateTexture2D(&shadowDesc, 0, shadowTexture.GetAddressOf());

		// Create the depth/stencil view
		constexpr D3D11_DEPTH_STENCIL_VIEW_DESC shadowDSDesc = {
			.Format = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
			.Texture2D = {.MipSlice = 0 },
		};
		m_shadowMapDepths.emplace_back();
		Graphics::Device->CreateDepthStencilView(
			shadowTexture.Get(),
			&shadowDSDesc,
			m_shadowMapDepths.back().GetAddressOf());

		// Create the SRV for the shadow map
		constexpr D3D11_SHADER_RESOURCE_VIEW_DESC shadowSRVDesc = {
			.Format = DXGI_FORMAT_R32_FLOAT,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = 1,
			},
		};
		m_shadowMaps.emplace_back();
		Graphics::Device->CreateShaderResourceView(
			shadowTexture.Get(),
			&shadowSRVDesc,
			m_shadowMaps.back().GetAddressOf());

		(*m_shadowMapResources)[i] = ShadowMapResources{ 
			.shaderResourceView = m_shadowMaps.back().Get(),
			.depthStencilView = m_shadowMapDepths.back().Get(),
		};
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

	constexpr D3D11_SAMPLER_DESC shadowMapSamplerDescription{
		.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D11_TEXTURE_ADDRESS_BORDER,
		.AddressV = D3D11_TEXTURE_ADDRESS_BORDER,
		.AddressW = D3D11_TEXTURE_ADDRESS_BORDER,
		.ComparisonFunc = D3D11_COMPARISON_LESS,
		.BorderColor = { 1.f, 1.f, 1.f, 1.f },
	};
	const auto shadowMapSamplerRes = Graphics::Device->CreateSamplerState(&shadowMapSamplerDescription, m_shadowMapSamplerState.GetAddressOf());
	gassert(shadowMapSamplerRes == S_OK);

	// Sampler state for post processing
	{
		constexpr D3D11_SAMPLER_DESC ppSampDesc = {
			.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
			.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
			.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
			.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
			.MaxLOD = D3D11_FLOAT32_MAX,
		};
		const auto res = Graphics::Device->CreateSamplerState(&ppSampDesc, m_postProcessSamplerState.GetAddressOf());
		gassert(res == S_OK);
	}
}

void ggp::Game::CreateRenderTarget()
{
	const D3D11_TEXTURE2D_DESC textureDesc = {
		.Width = Window::Width(),
		.Height = Window::Height(),
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = DXGI_SAMPLE_DESC{
			.Count = 1,
			.Quality = 0,
		},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
	};
	com_p<ID3D11Texture2D> ppTexture;
	Graphics::Device->CreateTexture2D(&textureDesc, nullptr, ppTexture.GetAddressOf());

	const D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {
		.Format = textureDesc.Format,
		.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
		.Texture2D = {.MipSlice = 0 },
	};

	m_postProcessRenderTargetView.Reset();
	m_postProcessShaderResourceView.Reset();

	Graphics::Device->CreateRenderTargetView(
		ppTexture.Get(), &rtvDesc, m_postProcessRenderTargetView.ReleaseAndGetAddressOf());
	Graphics::Device->CreateShaderResourceView(
		ppTexture.Get(), nullptr, m_postProcessShaderResourceView.ReleaseAndGetAddressOf());
}

void ggp::Game::LoadShaders()
{
	m_vertexShader = std::make_unique<SimpleVertexShader>(
		Graphics::Device, Graphics::Context, FixPath(L"forward_vs_base.cso").c_str());
	m_pixelShader = std::make_unique<SimplePixelShader>(
		Graphics::Device, Graphics::Context, FixPath(L"forward_ps_pbr.cso").c_str());
	m_shadowMapVertexShader = std::make_unique<SimpleVertexShader>(
		Graphics::Device, Graphics::Context, FixPath(L"shadowmap_vs.cso").c_str());
	defaultPixelShader = m_pixelShader.get();
	defaultVertexShader = m_vertexShader.get();
	m_postProcessPixelShader = std::make_unique<SimplePixelShader>(
		Graphics::Device, Graphics::Context, FixPath(L"post_process_ps_blur.cso").c_str());
	m_postProcessVertexShader = std::make_unique<SimpleVertexShader>(
		Graphics::Device, Graphics::Context, FixPath(L"post_process_vs.cso").c_str());
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

	Entity floor(quad, m_materials.at("wood").get(),  "wood floor");

	// recursively position all the entites around each other
	PositionEntities(root.GetTransform());

	m_entities.push_back(root);
	m_entities.push_back(layer00);
	m_entities.push_back(layer01);
	m_entities.push_back(out1);
	m_entities.push_back(out2);
	m_entities.push_back(out3);
	m_entities.push_back(droplet1);
	m_entities.push_back(droplet2);
	m_entities.push_back(droplet3);
	m_entities.push_back(floor);

	// floor has hardcoded position
	floor.GetTransform().SetPosition({ 0, -5, 0 });
	floor.GetTransform().SetScale({ 30, 1, 30 });
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

	if (Graphics::Device)
		CreateRenderTarget();
}

void ggp::Game::Update(float deltaTime, float totalTime)
{
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	if (m_spinningEnabled)
	{
		Transform root = m_entities[0].GetTransform();
		SpinRecursive(deltaTime, totalTime, root);
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

	ImGui::SliderInt("Blur radius", &m_blurRadius, 0, 100);

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
		Light& light = (*m_lights)[i];
		ImGui::ColorEdit3(buf.data(), &light.color.x);
		if (light.isShadowCaster) {
			ImGui::Image((*m_shadowMapResources)[i]->shaderResourceView, { 512, 512 });
		}
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

void ggp::Game::RenderShadowMaps() noexcept
{
	// all shadow maps are the same size
	D3D11_VIEWPORT viewport = {
		.Width = (float)shadowMapResolution,
		.Height = (float)shadowMapResolution,
		.MaxDepth = 1.0f,
	};
	Graphics::Context->RSSetViewports(1, &viewport);
	Graphics::Context->RSSetState(m_shadowMapRasterizerState.Get());

	// render all entities for every shadow map
	for (size_t i = 0; i < m_lights->size(); ++i) {
		const Light& light = (*m_lights)[i];
		if (!light.isShadowCaster)
			continue;

		// if this throws, shadow maps didnt get initialized properly
		const ShadowMapResources& res = (*m_shadowMapResources)[i].value();

		Graphics::Context->ClearDepthStencilView(res.depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
		ID3D11RenderTargetView* nullRTV {};
		Graphics::Context->OMSetRenderTargets(1, &nullRTV, res.depthStencilView);
		Graphics::Context->PSSetShader(nullptr, nullptr, 0);

		m_shadowMapVertexShader->SetShader();
		m_shadowMapVertexShader->SetMatrix4x4("view", light.shadowView);
		m_shadowMapVertexShader->SetMatrix4x4("projection", light.shadowProjection);

		for (auto& e : m_entities)
		{
			m_shadowMapVertexShader->SetMatrix4x4("world", *e.GetTransform().GetWorldMatrixPtr());
			m_shadowMapVertexShader->CopyAllBufferData();
			e.GetMesh()->BindBuffersAndDraw();
		}
	}

	viewport.Width = f32(Window::Width());
	viewport.Height = f32(Window::Height());
	Graphics::Context->RSSetViewports(1, &viewport);
	Graphics::Context->OMSetRenderTargets(
		1,
		Graphics::BackBufferRTV.GetAddressOf(),
		Graphics::DepthBufferDSV.Get());
	Graphics::Context->RSSetState(0);
}

void ggp::Game::Draw(float deltaTime, float totalTime)
{
	RenderShadowMaps();

	// Clear the back buffer (erase what's on screen) and depth buffer
	Graphics::Context->ClearRenderTargetView(Graphics::BackBufferRTV.Get(), m_backgroundColor.data());
	Graphics::Context->ClearDepthStencilView(Graphics::DepthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	Graphics::Context->ClearRenderTargetView(m_postProcessRenderTargetView.Get(), m_backgroundColor.data());
	Graphics::Context->OMSetRenderTargets(1, m_postProcessRenderTargetView.GetAddressOf(), Graphics::DepthBufferDSV.Get());

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
			vs->SetMatrix4x4("lightView", (*m_lights)[0].shadowView);
			vs->SetMatrix4x4("lightProjection", (*m_lights)[0].shadowProjection);

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

			ps->SetSamplerState("shadowSampler", m_shadowMapSamplerState);
			ps->SetShaderResourceView("shadowMap", (*m_shadowMapResources)[0]->shaderResourceView);

			vs->CopyAllBufferData();
			ps->CopyAllBufferData();
		}
		entity.GetMesh()->BindBuffersAndDraw();
	}

	m_skybox->Draw(m_skyboxResources, *camera.GetViewMatrix(), *camera.GetProjectionMatrix());

	// apply post process
	// restore back buffer so we draw to screen
	Graphics::Context->OMSetRenderTargets(1, Graphics::BackBufferRTV.GetAddressOf(), nullptr);

	auto& ps = *m_postProcessPixelShader;
	ps.SetShader();
	m_postProcessVertexShader->SetShader();

	ps.SetInt("blurRadius", m_blurRadius);
	ps.SetFloat("pixelWidth", 1.0f / f32(Window::Width()));
	ps.SetFloat("pixelHeight", 1.0f / f32(Window::Height()));
	ps.SetShaderResourceView("gameRenderTarget", m_postProcessShaderResourceView.Get());
	ps.SetSamplerState("postProcessSampler", m_postProcessSamplerState.Get());
	ps.CopyAllBufferData();
	m_postProcessVertexShader->CopyAllBufferData();
	Graphics::Context->Draw(3, 0); // Draw exactly 3 vertices (one triangle)

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

	static ID3D11ShaderResourceView* nullSRVs[128] = {};
	Graphics::Context->PSSetShaderResources(0, 128, nullSRVs);
}



