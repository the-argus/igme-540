#include "MapParser.h"
#include "ggp_dict.h"
#include <array>
#include <optional>
#include <variant>
#include <string>
#include <ranges>
#include <DirectXMath.h>

using namespace DirectX;

namespace ggp::MapParser
{
	enum class Scope
	{
		FILE,
		COMMENT,
		ENTITY,
		PROPERTY_VALUE,
		BRUSH,
		PLANE_0,
		PLANE_1,
		PLANE_2,
		TEXTURE,
		U,
		V,
		VALVE_U,
		VALVE_V,
		ROT,
		U_SCALE,
		V_SCALE
	};

	using Variant = std::variant<i32, f32, std::string, bool>;

	struct FacePoints
	{
		std::array<XMFLOAT3, 3> v;
	};

	struct ValveTextureAxis
	{
		XMFLOAT3 axis;
		f32 offset;
	};

	struct ValveUV
	{
		ValveTextureAxis u;
		ValveTextureAxis v;
	};

	struct FaceUVExtra
	{
		f32 rot;
		f32 scaleX;
		f32 scaleY;
	};

	struct Face 
	{
		FacePoints planePoints;
		XMFLOAT3 planeNormal;
		f32 planeDistance;
		u64 textureIndex;
		XMFLOAT2 uvStandard;
		std::optional<ValveUV> uvValve;
		FaceUVExtra uvExtra;
	};

	struct Brush
	{
		std::vector<Face> faces;
		XMFLOAT3 center;
	};

	struct MapEntity
	{
		dict<Variant> properties;
		std::vector<Brush> brushes;
		XMFLOAT3 center;
	};

	struct FaceVertex
	{
		XMFLOAT3 vertex;
		XMFLOAT3 normal;
		XMFLOAT2 uv;
		XMFLOAT4 tangent;
	};

	struct FaceGeometry
	{
		std::vector<FaceVertex> vertices;
		std::vector<u64> indices;
	};

	struct BrushGeometry
	{
		std::vector<FaceGeometry> faces;
	};

	struct MapEntityGeometry
	{
		std::vector<BrushGeometry> brushes;
	};

	struct TextureData
	{
		std::string name;
		u32 width;
		u32 height;
	};

	struct MapData
	{
		std::vector<MapEntity> entities;
		std::vector<MapEntityGeometry> entityGeometry;
		std::vector<TextureData> textures;

		u64 registerTexture(std::string_view name)
		{
			for (u64 i = 0; i < textures.size(); ++i)
			{
				if (textures[i].name == name)
					return i;
			}
			// NOTE: texture only partially initialized here, need to set width / height
			textures.emplace_back(name);
			return textures.size() - 1;
		}
	};

	std::vector<Mesh> parse(std::ifstream& file)
	{
		auto scope = Scope::FILE;
		bool isComment = false;
		std::optional<u32> entityIndex;
		std::optional<u32> brushIndex;
		std::optional<u32> faceIndex;
		std::optional<u32> componentIndex;
		std::string propertyKey;
		std::string currentProperty;
		bool isValveUVs = false;
		Face currentFace{};
		Brush currentBrush{};
		MapEntity currentEntity{};
		MapData mapData{};

		const auto submitCurrentBrushToCurrentEntity = [&]()
		{
			currentEntity.brushes.push_back(std::move(currentBrush));
			currentBrush = {};
		};

		const auto submitCurrentEntityToMapData = [&]()
		{
			if (currentEntity.properties.contains(std::string("_tb_type")) && !mapData.entities.empty())
			{
				auto& brushes = mapData.entities.front().brushes;
				// append currentEntity.brushes to the back of brushes
				brushes.insert(
					std::end(brushes),
					std::make_move_iterator(std::begin(currentEntity.brushes)),
					std::make_move_iterator(std::end(currentEntity.brushes)));
				currentEntity.brushes.clear();
			}
			mapData.entities.push_back(std::move(currentEntity));
			currentEntity = {};
		};

		const auto setCurrentVectorComponentForFacePoint = [&](std::string_view token, u8 facePointIndex)
		{
			switch (componentIndex.value())
			{
			case 0:
				currentFace.planePoints.v[facePointIndex].x = std::stof(std::string(token));
				break;
			case 1:
				currentFace.planePoints.v[facePointIndex].y = std::stof(std::string(token));
				break;
			case 2:
				currentFace.planePoints.v[facePointIndex].z = std::stof(std::string(token));
				break;
			}
		};

		enum class UV
		{
			U,
			V,
		};

		const auto setCurrentVectorComponentForValveUV = [&](std::string_view token, u8 facePointIndex, UV uv)
		{
			// by default the valve UV is zeroed and in invalid state
			if (!currentFace.uvValve)
				currentFace.uvValve = ValveUV{};

			ValveTextureAxis& component = uv == UV::U ? currentFace.uvValve->u : currentFace.uvValve->v;

			switch (componentIndex.value())
			{
			case 0:
				component.axis.x = std::stof(std::string(token));
				break;
			case 1:
				component.axis.y = std::stof(std::string(token));
				break;
			case 2:
				component.axis.z = std::stof(std::string(token));
				break;
			case 3:
				component.offset = std::stof(std::string(token));
				break;
			}
		};

		const auto submitCurrentFaceToCurrentBrush = [&]()
		{
			const XMVECTOR v0 = XMLoadFloat3(&currentFace.planePoints.v[0]);
			XMVECTOR v0v1 = XMLoadFloat3(&currentFace.planePoints.v[1]);
			XMVectorSubtract(v0v1, v0);
			XMVECTOR v1v2 = XMLoadFloat3(&currentFace.planePoints.v[2]);
			XMVectorSubtract(v1v2, XMLoadFloat3(&currentFace.planePoints.v[1]));

			XMStoreFloat3(&currentFace.planeNormal, XMVector3Normalize(XMVector3Cross(v1v2, v0v1)));
			currentFace.planeDistance = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&currentFace.planeNormal), v0));
			assert(currentFace.uvValve.has_value() == isValveUVs);
			currentBrush.faces.push_back(std::move(currentFace));
			currentFace = {};
		};

		const auto setScope = [&scope](Scope s) { scope = s; };

		const auto planeComponent = [&](std::string_view token, Scope nextScope, u8 faceScopeIndex)
		{
			if (token == ")")
			{
				componentIndex = 0;
				setScope(nextScope);
			}
			else
			{
				setCurrentVectorComponentForFacePoint(token, faceScopeIndex);
				*componentIndex += 1;
			}
		};

		const auto token = [&](std::string_view token)
		{
			if (isComment)
			{
				return;
			}
			else if (token == "//")
			{
				isComment = true;
				return;
			}

			switch (scope)
			{
			case Scope::FILE:
			{
				if (token == "{")
				{
					*entityIndex += 1;
					brushIndex = {};
					setScope(Scope::ENTITY);
				}
				break;
			}
			case Scope::ENTITY:
			{
				if (token[0] == '\"')
				{
					propertyKey = token.substr(1);
					if (token.back() == '\"')
					{
						propertyKey = propertyKey.substr(0, propertyKey.length() - 1);
						setScope(Scope::PROPERTY_VALUE);
					}
				}
				else if (token == "{")
				{
					*brushIndex += 1;
					faceIndex = {};
					setScope(Scope::BRUSH);
				}
				else if (token == "}")
				{
					submitCurrentEntityToMapData();
					setScope(Scope::FILE);
				}
				break;
			}
			case Scope::PROPERTY_VALUE:
			{
				const bool isFirst = token.front() == '"';
				const bool isLast = token.back() == '"';

				if (isFirst && !currentProperty.empty())
					currentProperty.clear();

				currentProperty += std::string(token) + (!isLast ? " " : "");

				if (isLast)
				{
					currentEntity.properties[propertyKey] = currentProperty.substr(1, currentProperty.length() - 2);
					setScope(Scope::ENTITY);
				}
				break;
			}
			case Scope::BRUSH:
			{
				if (token == "(")
				{
					*faceIndex += 1;
					componentIndex = 0;
					setScope(Scope::PLANE_0);
				}
				else if (token == "}")
				{
					submitCurrentBrushToCurrentEntity();
					setScope(Scope::ENTITY);
				}
				break;
			}
			case Scope::PLANE_0:
				planeComponent(token, Scope::PLANE_1, 0);
				break;
			case Scope::PLANE_1:
				if (token != "(")
					planeComponent(token, Scope::PLANE_2, 1);
				break;
			case Scope::PLANE_2:
				if (token != "(")
					planeComponent(token, Scope::TEXTURE, 2);
				break;
			case Scope::TEXTURE:
			{
				currentFace.textureIndex = mapData.registerTexture(token);
				setScope(Scope::U);
				break;
			}
			case Scope::U:
			{
				if (token == "[")
				{
					isValveUVs = true;
					componentIndex = 0;
					setScope(Scope::VALVE_U);
					break;
				}
				isValveUVs = false;
				currentFace.uvStandard.x = std::stof(std::string(token));
				setScope(Scope::V);
				break;
			}
			case Scope::V:
			{
				currentFace.uvStandard.y = std::stof(std::string(token));
				setScope(Scope::ROT);
				break;
			}
			case Scope::VALVE_U:
			{
				if (token == "]")
				{
					componentIndex = 0;
					setScope(Scope::VALVE_V);
					break;
				}
				setCurrentVectorComponentForValveUV(token, componentIndex.value(), UV::U);
				*componentIndex += 1;
				break;
			}
			case Scope::VALVE_V:
			{
				if (token == "[")
					break;

				if (token == "]")
				{
					setScope(Scope::ROT);
					break;
				}

				setCurrentVectorComponentForValveUV(token, componentIndex.value(), UV::V);
				*componentIndex += 1;
				break;
			}
			case Scope::ROT:
			{
				currentFace.uvExtra.rot = std::stof(std::string(token));
				setScope(Scope::U_SCALE);
				break;
			}
			case Scope::U_SCALE:
			{
				currentFace.uvExtra.scaleX = std::stof(std::string(token));
				setScope(Scope::V_SCALE);
				break;
			}
			case Scope::V_SCALE:
			{
				currentFace.uvExtra.scaleY = std::stof(std::string(token));
				submitCurrentFaceToCurrentBrush();
				setScope(Scope::BRUSH);
			};
			}
		};

		std::string line;
		while (std::getline(file, line))
		{
			auto unifyWhitespace = [](char c) { return c == '\t' ? ' ' : c; };

			auto tokens = line
				| std::views::transform(unifyWhitespace)
				| std::views::split(' ');

			for (const auto& t : tokens)
				// TODO: make string view instead of string here?
				token(std::string(std::begin(t), std::end(t)));
		}
		return {};
	}
}