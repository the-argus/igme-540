#pragma once

#include "Entity.h"
#include "ggp_dict.h"
#include <fstream>

namespace ggp::MapParser
{
	struct MapSettings
	{
		// relative to executable
		std::string_view baseTexturesDir = "../../assets";
		std::string_view clipTexture = "special/clip";
		std::string_view skipTexture = "special/skip";
		const com_p<ID3D11ShaderResourceView>& defaultTexture;
		SimplePixelShader* pbrPixelShader;
		SimpleVertexShader* pbrVertexShader;
		ID3D11SamplerState* pbrTextureSampler;
		f32 scaleFactor = 1;
		// bool useTrenchBroomGroupsHierarchy = false;
	};

	struct MapResult
	{
		std::vector<Entity> elements;
		Entity mapRoot;
		dict<std::unique_ptr<Mesh>> meshes;
		dict<std::unique_ptr<Material>> materials;
		dict<com_p<ID3D11ShaderResourceView>> textureViews;
	};

	// this function uses the currently active TransformHierarchy singleton
	MapResult parse(std::ifstream& file, const MapSettings& settings);
}