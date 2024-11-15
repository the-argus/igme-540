#include "Sky.h"
#include "Graphics.h"
#include "errors.h"
#include <WICTextureLoader.h>

ggp::Sky::Sky(ID3D11ShaderResourceView* skyboxCubemapTextureView, ID3D11SamplerState* skyboxSampler) noexcept :
	m_cubemap(skyboxCubemapTextureView),
	m_sampler(skyboxSampler)
{
}

void ggp::Sky::Draw(const SharedResources& skyboxResources, DirectX::XMFLOAT4X4 viewMatrix, DirectX::XMFLOAT4X4 projectionMatrix) const noexcept
{
	Graphics::Context->RSSetState(skyboxResources.rasterizerState.Get());
	Graphics::Context->OMSetDepthStencilState(skyboxResources.depthStencilState.Get(), 0);
	
	skyboxResources.skyboxPixelShader->SetShader();
	skyboxResources.skyboxVertexShader->SetShader();

	skyboxResources.skyboxVertexShader->SetMatrix4x4("viewMatrix", viewMatrix);
	skyboxResources.skyboxVertexShader->SetMatrix4x4("projectionMatrix", projectionMatrix);

	skyboxResources.skyboxPixelShader->SetShaderResourceView("skybox", m_cubemap);
	skyboxResources.skyboxPixelShader->SetSamplerState("skyboxSampler", m_sampler);

	skyboxResources.skyboxPixelShader->CopyAllBufferData();
	skyboxResources.skyboxVertexShader->CopyAllBufferData();

	skyboxResources.skyMesh->BindBuffersAndDraw();

	Graphics::Context->RSSetState(0);
	Graphics::Context->OMSetDepthStencilState(0, 0);
}

auto ggp::Sky::CreateDepthStencilStateThatKeepsDeepPixels() noexcept -> com_p<ID3D11DepthStencilState>
{
	constexpr D3D11_DEPTH_STENCIL_DESC depthStencilDescription = {
		.DepthEnable = true,
		.DepthFunc = D3D11_COMPARISON_LESS_EQUAL,
	};
	com_p<ID3D11DepthStencilState> out;
	const auto result = Graphics::Device->CreateDepthStencilState(&depthStencilDescription, out.GetAddressOf());
	gassert(result == S_OK);
	gassert(out);
	return out;
}

auto ggp::Sky::CreateRasterizerStateThatDrawsBackfaces() noexcept -> com_p<ID3D11RasterizerState>
{
	constexpr D3D11_RASTERIZER_DESC rasterizerDescription = {
		.FillMode = D3D11_FILL_SOLID,
		.CullMode = D3D11_CULL_FRONT,
	};
	com_p<ID3D11RasterizerState> out;
	const auto result = Graphics::Device->CreateRasterizerState(&rasterizerDescription, out.GetAddressOf());
	gassert(result == S_OK);
	gassert(out);
	return out;
}

// --------------------------------------------------------
// Author: Chris Cascioli
// Purpose: Creates a cube map on the GPU from 6 individual textures
// 
// - You are allowed to directly copy/paste this into your code base
//   for assignments, given that you clearly cite that this is not
//   code of your own design.
// --------------------------------------------------------
auto ggp::Sky::LoadCubemap(const LoadCubemapOptions& options) -> com_p<ID3D11ShaderResourceView>
{
	// Load the 6 textures into an array.
	// - We need references to the TEXTURES, not SHADER RESOURCE VIEWS!
	// - Explicitly NOT generating mipmaps, as we don't need them for the sky!
	// - Order matters here!  +X, -X, +Y, -Y, +Z, -Z
	com_p<ID3D11Texture2D> textures[6] = {};
	using namespace DirectX;
	CreateWICTextureFromFile(Graphics::Device.Get(), options.right, (ID3D11Resource**)textures[0].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), options.left, (ID3D11Resource**)textures[1].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), options.up, (ID3D11Resource**)textures[2].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), options.down, (ID3D11Resource**)textures[3].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), options.front, (ID3D11Resource**)textures[4].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), options.back, (ID3D11Resource**)textures[5].GetAddressOf(), 0);

	// We'll assume all of the textures are the same color format and resolution,
	// so get the description of the first texture
	D3D11_TEXTURE2D_DESC faceDesc = {};
	textures[0]->GetDesc(&faceDesc);

	// Describe the resource for the cube map, which is simply 
	// a "texture 2d array" with the TEXTURECUBE flag set.  
	// This is a special GPU resource format, NOT just a 
	// C++ array of textures!!!
	D3D11_TEXTURE2D_DESC cubeDesc = {};
	cubeDesc.ArraySize = 6;            // Cube map!
	cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // We'll be using as a texture in a shader
	cubeDesc.CPUAccessFlags = 0;       // No read back
	cubeDesc.Format = faceDesc.Format; // Match the loaded texture's color format
	cubeDesc.Width = faceDesc.Width;   // Match the size
	cubeDesc.Height = faceDesc.Height; // Match the size
	cubeDesc.MipLevels = 1;            // Only need 1
	cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // This should be treated as a CUBE, not 6 separate textures
	cubeDesc.Usage = D3D11_USAGE_DEFAULT; // Standard usage
	cubeDesc.SampleDesc.Count = 1;
	cubeDesc.SampleDesc.Quality = 0;

	// Create the final texture resource to hold the cube map
	Microsoft::WRL::ComPtr<ID3D11Texture2D> cubeMapTexture;
	Graphics::Device->CreateTexture2D(&cubeDesc, 0, cubeMapTexture.GetAddressOf());

	// Loop through the individual face textures and copy them,
	// one at a time, to the cube map texure
	for (int i = 0; i < 6; i++)
	{
		// Calculate the subresource position to copy into
		unsigned int subresource = D3D11CalcSubresource(
			0,  // Which mip (zero, since there's only one)
			i,  // Which array element?
			1); // How many mip levels are in the texture?

		// Copy from one resource (texture) to another
		Graphics::Context->CopySubresourceRegion(
			cubeMapTexture.Get(),  // Destination resource
			subresource,           // Dest subresource index (one of the array elements)
			0, 0, 0,               // XYZ location of copy
			textures[i].Get(),     // Source resource
			0,                     // Source subresource index (we're assuming there's only one)
			0);                    // Source subresource "box" of data to copy (zero means the whole thing)
	}

	// At this point, all of the faces have been copied into the 
	// cube map texture, so we can describe a shader resource view for it
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = cubeDesc.Format;         // Same format as texture
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE; // Treat this as a cube!
	srvDesc.TextureCube.MipLevels = 1;        // Only need access to 1 mip
	srvDesc.TextureCube.MostDetailedMip = 0;  // Index of the first mip we want to see

	// Make the SRV
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeSRV;
	Graphics::Device->CreateShaderResourceView(cubeMapTexture.Get(), &srvDesc, cubeSRV.GetAddressOf());

	// Send back the SRV, which is what we need for our shaders
	return cubeSRV;
}
