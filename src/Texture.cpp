#include "Texture.h"
#include "Graphics.h"
#include "errors.h"
#include "PathHelpers.h"

#include <WICTextureLoader.h>

using namespace DirectX;

void ggp::Texture::LoadPNG(ID3D11ShaderResourceView** out_srv, const char* texturename, std::wstring subDir)
{
	using namespace ggp;
	const auto wideName = std::wstring(texturename, texturename + std::strlen(texturename));
	const auto path = subDir + wideName + std::wstring(L".png");
	const auto result = CreateWICTextureFromFile(
		Graphics::Device.Get(),
		Graphics::Context.Get(),
		FixPath(path).c_str(),
		nullptr, out_srv);
	gassert(result == S_OK);
}
