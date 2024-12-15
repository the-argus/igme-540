#pragma once
#include <d3d11.h>
#include <string>

namespace ggp::Texture
{
	void LoadPNG(
		ID3D11ShaderResourceView** out_srv,
		const char* texturename,
		std::wstring subdir = L"assets/example_textures/ ");
}