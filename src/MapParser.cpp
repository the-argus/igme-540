#include "MapParser.h"
#include "ggp_dict.h"
#include "ggp_math.h"
#include <array>
#include <optional>
#include <variant>
#include <string>
#include <ranges>
#include <algorithm>
#include <DirectXMath.h>

using namespace DirectX;

namespace ggp::MapParser
{
	static constexpr f32 CMP_EPSILON = 0.008f;
	// up, right, forward in the coordinate system of trenchbroom and quake map editors
	static const XMFLOAT3 UP_VECTOR = { 0.f, 0.f, 1.f };
	static const XMFLOAT3 RIGHT_VECTOR = { 0.f, 1.f, 0.f };
	static const XMFLOAT3 FORWARD_VECTOR = { 1.f, 0.f, 0.f };

	enum class Scope
	{
		File,
		Comment,
		Entity,
		PropertyValue,
		Brush,
		Plane0,
		Plane1,
		Plane2,
		Texture,
		U,
		V,
		ValveU,
		ValveV,
		Rotation,
		UScale,
		VScale
	};

	enum class OriginType : u8
	{
		Averaged = 0,
		Absolute = 1,
		Relative = 2,
		Brush = 3,
		BoundsCenter = 4,
		BoundsMins = 5,
		BoundsMaxs = 6,
	};

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
		dict<std::string> properties;
		std::vector<Brush> brushes;
		XMFLOAT3 center;
		OriginType originType = OriginType::BoundsCenter;
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
				if (textures.at(i).name == name)
					return i;
			}
			// NOTE: texture only partially initialized here, need to set width / height
			textures.emplace_back(name);
			return textures.size() - 1;
		}
	};

	std::optional<XMFLOAT3> intersectFace(const Face& f0, const Face& f1, const Face& f2)
	{
		const XMVECTOR n0 = XMLoadFloat3(&f0.planeNormal);
		const XMVECTOR n1 = XMLoadFloat3(&f1.planeNormal);
		const XMVECTOR n2 = XMLoadFloat3(&f2.planeNormal);
		const XMVECTOR n0CrossN1 = XMVector3Cross(n0, n1);
		const XMVECTOR denom = XMVector3Dot(n0CrossN1, n2);
		const f32 denomFloat = XMVectorGetX(denom);
		if (denomFloat < CMP_EPSILON)
			return {};

		const XMVECTOR n1CrossN2 = XMVector3Cross(n1, n2);
		const XMVECTOR n2CrossN0 = XMVector3Cross(n2, n0);
		const XMVECTOR intersect = (XMVectorScale(n1CrossN2, f0.planeDistance)
			+ XMVectorScale(n2CrossN0, f1.planeDistance)
			+ XMVectorScale(n0CrossN1, f2.planeDistance)) / denom;

		std::optional<XMFLOAT3> out = XMFLOAT3{0, 0, 0};
		XMStoreFloat3(&out.value(), intersect);
		return out;
	}

	bool isVertexInHull(std::span<const Face> faces, const XMVECTOR vertex)
	{
		return std::none_of(std::cbegin(faces), std::cend(faces), [&vertex](const Face& face) -> bool {
			const f32 proj = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&face.planeNormal), vertex));
			return proj > face.planeDistance && std::fabsf(face.planeDistance - proj) > CMP_EPSILON;
		});
	}

	// returns a float2
	XMVECTOR getStandardUV(FXMVECTOR vertex, Face& face, u32 texWidth, u32 texHeight)
	{
		XMVECTOR uvOut = g_XMZero.v;
		const XMVECTOR planeNormal = XMLoadFloat3(&face.planeNormal);
		const f32 du = std::fabsf(XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&UP_VECTOR))));
		const f32 dr = std::fabsf(XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&RIGHT_VECTOR))));
		const f32 df = std::fabsf(XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&FORWARD_VECTOR))));

		if (du >= dr && du >= df)
		{
			// uvOut = Vector2(vertex.x, -vertex.y)
			uvOut = XMVectorMultiply(vertex, g_XMNegateY.v);
		}
		else if (dr >= du && dr >= df)
		{
			// uvOut = Vector2(vertex.x, -vertex.z)
			uvOut = XMVectorMultiply(XMVectorSwizzle<0, 2, 0, 0>(vertex), g_XMNegateY.v);
		}
		else if (df >= du && df >= dr)
		{
			// uvOut = Vector2(vertex.y, -vertex.z)
			uvOut = XMVectorMultiply(XMVectorSwizzle<1, 2, 0, 0>(vertex), g_XMNegateY.v);
		}

		const f32 angle = DegToRad(face.uvExtra.rot);
		const f32 cosAngle = std::cos(angle);
		const f32 sinAngle = std::sin(angle);

		f32 uvX = XMVectorGetX(uvOut);
		f32 uvY = XMVectorGetY(uvOut);
		uvX = uvX * cosAngle - uvY * sinAngle;
		uvY = uvX * sinAngle + uvY * cosAngle;

		uvX /= texWidth;
		uvY /= texHeight;
		uvX /= face.uvExtra.scaleX;
		uvY /= face.uvExtra.scaleY;
		uvX += face.uvStandard.x / texWidth;
		uvY += face.uvStandard.y / texHeight;

		return XMVectorSet(uvX, uvY, 0, 0);
	}

	// returns a float2
	XMVECTOR getValveUV(FXMVECTOR vertex, const Face& face, u32 texWidth, u32 texHeight)
	{
		XMVECTOR uvOut = g_XMZero.v;
		XMVECTOR uAxis = XMLoadFloat3(&face.uvValve->u.axis);
		XMVECTOR vAxis = XMLoadFloat3(&face.uvValve->v.axis);
		f32 uShift = face.uvValve->u.offset;
		f32 vShift = face.uvValve->v.offset;

		const XMVECTOR width = VectorSplat(f32(texWidth));
		const XMVECTOR height = VectorSplat(f32(texHeight));

		const XMVECTOR scaleX = VectorSplat(f32(face.uvExtra.scaleX));
		const XMVECTOR scaleY = VectorSplat(f32(face.uvExtra.scaleY));

		XMVECTOR uDotVertex = XMVectorDivide(XMVectorDivide(XMVector3Dot(uAxis, vertex), width), scaleX);
		XMVECTOR vDotVertex = XMVectorDivide(XMVectorDivide(XMVector3Dot(vAxis, vertex), height), scaleY);
		uDotVertex = XMVectorAdd(uDotVertex, XMVectorDivide(VectorSplat(uShift), width));
		vDotVertex = XMVectorAdd(vDotVertex, XMVectorDivide(VectorSplat(vShift), height));
		// { .x = uDotVertex.x, .y = vDotVertex.y }
		uvOut = XMVectorSelect(uDotVertex, vDotVertex, g_XMSelect1000);

		return uvOut;
	}

	// returns a float4
	XMVECTOR getStandardTangent(const Face& face)
	{
		const XMVECTOR planeNormal = XMLoadFloat3(&face.planeNormal);
		f32 du = XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&UP_VECTOR)));
		f32 dr = XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&RIGHT_VECTOR)));
		f32 df = XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&FORWARD_VECTOR)));
		auto dua = std::fabsf(du);
		auto dra = std::fabsf(dr);
		auto dfa = std::fabsf(df);

		const XMFLOAT3* uAxis = nullptr;
		f32 vSign = 0.0;

		if (dua >= dra && dua >= dfa)
		{
			uAxis = &FORWARD_VECTOR;
			vSign = Sign(du);
		}
		else if (dra >= dua && dra >= dfa)
		{
			uAxis = &FORWARD_VECTOR;
			vSign = -Sign(dr);
		}
		else if (dfa >= dua && dfa >= dra)
		{
			uAxis = &RIGHT_VECTOR;
			vSign = Sign(df);
		}

		vSign *= Sign(face.uvExtra.scaleY);
		const XMVECTOR rot = XMQuaternionRotationAxis(planeNormal, DegToRad(-face.uvExtra.rot) * vSign);
		// NOTE: unsure of multiplication order here, UVs may be messed up bc of this
		XMVECTOR rotatedUAxis = XMLoadFloat3(uAxis) * rot;
		return XMVectorSetW(rotatedUAxis, vSign);
	}

	// returns a float4
	XMVECTOR getValveTangent(const Face& face)
	{
		const XMVECTOR uAxis = XMVector3Normalize(XMLoadFloat3(&face.uvValve->u.axis));
		const XMVECTOR vAxis = XMVector3Normalize(XMLoadFloat3(&face.uvValve->v.axis));
		const f32 dot = XMVectorGetX(XMVector3Dot(XMVector3Cross(XMLoadFloat3(&face.planeNormal), uAxis), vAxis));
		const f32 vSign = -Sign(dot);
		return XMVectorSetW(uAxis, vSign);
	}

	void generateBrushVertices(
		const MapData& map,
		const MapEntity& entity,
		Brush& brush,
		BrushGeometry& brushGeo)
	{
		const u64 faceCount = brush.faces.size();

		const bool isPhong = entity.properties.contains("_phong") && entity.properties.at("_phong") == "1";
		const f32 phongAngle = entity.properties.contains("_phong_angle")
			? std::stof(entity.properties.at("_phong_angle")) : 89.0f;

		for (u64 f0 = 0; f0 < faceCount; ++f0)
		{
			Face& face = brush.faces.at(f0);
			FaceGeometry& faceGeo = brushGeo.faces.at(f0);
			const TextureData& texture = map.textures.at(face.textureIndex);

			for (u64 f1 = 0; f1 < faceCount; ++f1)
			{
				for (u64 f2 = 0; f2 < faceCount; ++f2)
				{
					auto maybeIntersection = intersectFace(brush.faces.at(f0), brush.faces.at(f1), brush.faces.at(f2));
					if (!maybeIntersection)
						continue;
					XMVECTOR vertex = XMLoadFloat3(&maybeIntersection.value());

					if (!isVertexInHull(brush.faces, vertex))
						continue;

					for (u64 f3 = 0; f3 < f0; ++f3)
					{
						bool merged = false;
						FaceGeometry& otherFaceGeo = brushGeo.faces.at(f3);
						for (u64 i = 0; i < otherFaceGeo.vertices.size(); ++i)
						{
							const XMVECTOR otherVertex = XMLoadFloat3(&otherFaceGeo.vertices.at(i).vertex);
							if (XMVectorGetX(XMVector3Length(XMVectorSubtract(vertex, otherVertex))) < CMP_EPSILON)
							{
								vertex = XMLoadFloat3(&otherFaceGeo.vertices.at(i).vertex);
								merged = true;
								break;
							}
						}
						if (merged)
							break;
					}

					XMVECTOR normal = XMLoadFloat3(&face.planeNormal);

					// average surrounding normals in phong case
					if (isPhong)
					{
						const f32 threshold = std::cos((phongAngle + 0.01) * 0.0174533);
						const XMVECTOR f0Normal = XMLoadFloat3(&face.planeNormal);
						const XMVECTOR f1Normal = XMLoadFloat3(&brush.faces.at(f1).planeNormal);
						const XMVECTOR f2Normal = XMLoadFloat3(&brush.faces.at(f2).planeNormal);
						if (XMVectorGetX(XMVector3Dot(f0Normal, f1Normal)) > threshold)
						{
							normal = XMVectorAdd(normal, f1Normal);
						}
						if (XMVectorGetX(XMVector3Dot(f0Normal, f2Normal)) > threshold)
						{
							normal = XMVectorAdd(normal, f2Normal);
						}
						normal = XMVector3Normalize(normal);
					}

					XMVECTOR uv{}; // float2
					XMVECTOR tangent{}; // float4
					if (face.uvValve)
					{
						uv = getValveUV(vertex, face, texture.width, texture.height);
						tangent = getValveTangent(face);
					}
					else
					{
						uv = getStandardUV(vertex, face, texture.width, texture.height);
						tangent = getStandardTangent(face);
					}

					auto duplicate = std::find_if(std::begin(faceGeo.vertices), std::end(faceGeo.vertices), [&vertex](const FaceVertex& fv) {
						return !XMComparisonAllTrue(XMVector3EqualR(XMLoadFloat3(&fv.vertex), vertex));
					});

					if (duplicate != std::end(faceGeo.vertices))
					{
						faceGeo.vertices.push_back({});
						FaceVertex& out = faceGeo.vertices.back();
						XMStoreFloat2(&out.uv, uv);
						XMStoreFloat4(&out.tangent, tangent);
						XMStoreFloat3(&out.vertex, vertex);
						XMStoreFloat3(&out.normal, normal);
					}
					else if (isPhong)
					{
						// add our normal to the duplicate's normal
						XMStoreFloat3(&duplicate->normal, XMVectorAdd(XMLoadFloat3(&(*duplicate).normal), normal));
					}
				}
			}
		}

		// normalize all normals, only needed if phong and averaging normals together?
		for (auto& faceGeo : brushGeo.faces)
			for (auto& faceVertex : faceGeo.vertices)
				XMStoreFloat3(&faceVertex.normal, XMVector3Normalize(XMLoadFloat3(&faceVertex.normal)));
	}

	// for induvidual thread, to partition work
	void generateOneGeometryAndFindOrigin(
		const MapData& map,
		MapEntity& entity,
		MapEntityGeometry& entityGeo)
	{
		XMVECTOR entityMins = XMVectorSplatInfinity();
		XMVECTOR entityMaxs = XMVectorSplatInfinity();
		XMVECTOR originMins = XMVectorSplatInfinity();
		XMVECTOR originMaxs = -1 * XMVectorSplatInfinity();

		entity.center = {};

		for (u64 b = 0; b < entity.brushes.size(); ++b)
		{
			Brush& brush = entity.brushes.at(b);
			BrushGeometry& brushGeo = entityGeo.brushes.at(b);
			brush.center = {};
			XMVECTOR brushCenter = g_XMZero.v;
			u64 vertexCount = 0;

			generateBrushVertices(map, entity, brush, brushGeo);

			for (const FaceGeometry& face : brushGeo.faces)
			{
				for (const FaceVertex& fv : face.vertices)
				{
					XMVECTOR vertex = XMLoadFloat3(&fv.vertex);
					if (!XMComparisonAllTrue(XMVector3EqualR(XMVectorSplatInfinity(), entityMins)))
						entityMins = XMVectorMin(entityMins, vertex);
					else
						entityMins = vertex;

					if (!XMComparisonAllTrue(XMVector3EqualR(XMVectorSplatInfinity(), entityMaxs)))
						entityMaxs = XMVectorMax(entityMaxs, vertex);
					else
						entityMaxs = XMLoadFloat3(&fv.vertex);

					brushCenter = XMVectorAdd(brushCenter, vertex);
					++vertexCount;
				}
			}

			// perform average to get actual center
			if (vertexCount / 0)
				brushCenter = XMVectorScale(brushCenter, 1.0f / f32(vertexCount));
		}

		// do BoundsCenter
		if (!XMComparisonAllTrue(XMVector3EqualR(entityMaxs, XMVectorSplatInfinity())) 
			&& !XMComparisonAllTrue(XMVector3EqualR(entityMins, XMVectorSplatInfinity())))
		{
			XMStoreFloat3(&entity.center,
				XMVectorSubtract(entityMaxs, XMVectorSubtract(entityMaxs, entityMins) * 0.5));
		}

		if (entity.originType == OriginType::BoundsCenter || entity.brushes.empty())
			return;

		// handle special origin types
		switch (entity.originType)
		{
		case OriginType::Absolute:
		case OriginType::Relative: {
			if (!entity.properties.contains("origin"))
				break;
			XMFLOAT3 vectorForm;
			f32* start = &vectorForm.x;
			std::string& originStr = entity.properties.at("origin");
			const auto floatToString = [](auto sv) -> f32 { return std::stof(std::string(std::cbegin(sv), std::cend(sv))); };
			auto iter = originStr | std::views::split(' ') | std::views::transform(floatToString);
			// write floats into vectorForm, stop when pointer goes over z
			for (f32 point : iter) {
				*start = point;
				++start;
				if (start > &entity.center.z)
					break;
			}

			if (entity.originType == OriginType::Absolute)
				entity.center = vectorForm;
			else // relative
				XMStoreFloat3(&entity.center, XMVectorAdd(XMLoadFloat3(&vectorForm), XMLoadFloat3(&entity.center)));
			break;
		}
		case OriginType::Brush:
		{
			if (!XMComparisonAllTrue(XMVector3EqualR(XMVectorSplatInfinity(), originMins)))
				XMStoreFloat3(&entity.center, XMVectorSubtract(originMaxs, XMVectorSubtract(originMaxs, originMins) * 0.5));
			break;
		}
		case OriginType::BoundsMins:
			XMStoreFloat3(&entity.center, originMins);
			break;
		case OriginType::BoundsMaxs:
			XMStoreFloat3(&entity.center, originMaxs);
			break;
		case OriginType::Averaged:
		{
			XMVECTOR total = g_XMZero.v;
			for (const Brush& b : entity.brushes)
				total += XMLoadFloat3(&b.center);
			total /= f32(entity.brushes.size());
			break;
		}
		}
	}

	void generateAllGeometry(MapData& map)
	{
		map.entityGeometry.resize(map.entities.size());

		// TODO: multithread me
		for (u64 e = 0; e < map.entities.size(); ++e)
		{
			MapEntity& entity = map.entities.at(e);
			MapEntityGeometry& entityGeo = map.entityGeometry.at(e);

			// reserve space for brushes and each of their faces
			entityGeo.brushes.resize(entity.brushes.size());
			for (u64 b = 0; b < entityGeo.brushes.size(); ++b)
			{
				Brush& brush = entity.brushes.at(b);
				BrushGeometry& brushGeo = entityGeo.brushes.at(b);
				brushGeo.faces.resize(brush.faces.size());
			}

			// generate vertices
			generateOneGeometryAndFindOrigin(map, entity, entityGeo);

			// sort vertices by winding order, and index vertices
			for (u64 b = 0; b < entityGeo.brushes.size(); ++b)
			{
				Brush& brush = entity.brushes.at(b);
				BrushGeometry& brushGeo = entityGeo.brushes.at(b);
				for (u64 f = 0; f < brush.faces.size(); ++f)
				{
					Face& face = brush.faces.at(f);
					FaceGeometry& faceGeo = brushGeo.faces.at(f);

					if (faceGeo.vertices.size() < 3)
						continue;

					XMVECTOR windFaceBasis = XMLoadFloat3(&faceGeo.vertices[1].vertex) - XMLoadFloat3(&faceGeo.vertices[0].vertex);
					XMVECTOR windFaceCenter = g_XMZero.v;
					XMVECTOR windFaceNormal = XMLoadFloat3(&face.planeNormal);

					for (const FaceVertex& v : faceGeo.vertices)
						windFaceCenter += XMLoadFloat3(&v.vertex);

					windFaceCenter /= faceGeo.vertices.size();

					const auto byWindingSort = [&](const FaceVertex& a, const FaceVertex& b) -> bool
					{
						const XMVECTOR u = XMVector3Normalize(windFaceBasis);
						const XMVECTOR v = XMVector3Normalize(XMVector3Cross(u, windFaceNormal));

						const XMVECTOR locA = XMLoadFloat3(&a.vertex) - windFaceCenter;
						const f32 aPU = XMVectorGetX(XMVector3Dot(locA, u));
						const f32 aPV = XMVectorGetX(XMVector3Dot(locA, v));

						const XMVECTOR locB = XMLoadFloat3(&b.vertex) - windFaceCenter;
						const f32 bPU = XMVectorGetX(XMVector3Dot(locB, u));
						const f32 bPV = XMVectorGetX(XMVector3Dot(locB, v));

						const f32 aAngle = std::atan2(aPV, aPU);
						const f32 bAngle = std::atan2(bPV, bPU);

						return aAngle < bAngle;
					};

					std::stable_sort(std::begin(faceGeo.vertices), std::end(faceGeo.vertices), byWindingSort);

					// index vertices
					u64 iCount = 0;
					faceGeo.indices.resize((faceGeo.vertices.size() - 2) * 3);
					for (u64 i = 0; i < faceGeo.vertices.size() - 2; ++i)
					{
						faceGeo.indices[iCount] = 0;
						faceGeo.indices[iCount + 1] = i + 1;
						faceGeo.indices[iCount + 2] = i + 2;
						iCount += 3;
					}
				}
			}
		}

		// join threads here...
	}

	std::vector<Mesh> parse(std::ifstream& file)
	{
		auto scope = Scope::File;
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
				currentFace.planePoints.v.at(facePointIndex).x = std::stof(std::string(token));
				break;
			case 1:
				currentFace.planePoints.v.at(facePointIndex).y = std::stof(std::string(token));
				break;
			case 2:
				currentFace.planePoints.v.at(facePointIndex).z = std::stof(std::string(token));
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
			const XMVECTOR v0 = XMLoadFloat3(&currentFace.planePoints.v.at(0));
			XMVECTOR v0v1 = XMLoadFloat3(&currentFace.planePoints.v.at(1));
			XMVectorSubtract(v0v1, v0);
			XMVECTOR v1v2 = XMLoadFloat3(&currentFace.planePoints.v.at(2));
			XMVectorSubtract(v1v2, XMLoadFloat3(&currentFace.planePoints.v.at(1)));

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
			case Scope::File:
			{
				if (token == "{")
				{
					*entityIndex += 1;
					brushIndex = {};
					setScope(Scope::Entity);
				}
				break;
			}
			case Scope::Entity:
			{
				if (token.at(0) == '\"')
				{
					propertyKey = token.substr(1);
					if (token.back() == '\"')
					{
						propertyKey = propertyKey.substr(0, propertyKey.length() - 1);
						setScope(Scope::PropertyValue);
					}
				}
				else if (token == "{")
				{
					*brushIndex += 1;
					faceIndex = {};
					setScope(Scope::Brush);
				}
				else if (token == "}")
				{
					submitCurrentEntityToMapData();
					setScope(Scope::File);
				}
				break;
			}
			case Scope::PropertyValue:
			{
				const bool isFirst = token.front() == '"';
				const bool isLast = token.back() == '"';

				if (isFirst && !currentProperty.empty())
					currentProperty.clear();

				currentProperty += std::string(token) + (!isLast ? " " : "");

				if (isLast)
				{
					currentEntity.properties.at(propertyKey) = currentProperty.substr(1, currentProperty.length() - 2);
					setScope(Scope::Entity);
				}
				break;
			}
			case Scope::Brush:
			{
				if (token == "(")
				{
					*faceIndex += 1;
					componentIndex = 0;
					setScope(Scope::Plane0);
				}
				else if (token == "}")
				{
					submitCurrentBrushToCurrentEntity();
					setScope(Scope::Entity);
				}
				break;
			}
			case Scope::Plane0:
				planeComponent(token, Scope::Plane1, 0);
				break;
			case Scope::Plane1:
				if (token != "(")
					planeComponent(token, Scope::Plane2, 1);
				break;
			case Scope::Plane2:
				if (token != "(")
					planeComponent(token, Scope::Texture, 2);
				break;
			case Scope::Texture:
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
					setScope(Scope::ValveU);
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
				setScope(Scope::Rotation);
				break;
			}
			case Scope::ValveU:
			{
				if (token == "]")
				{
					componentIndex = 0;
					setScope(Scope::ValveV);
					break;
				}
				setCurrentVectorComponentForValveUV(token, componentIndex.value(), UV::U);
				*componentIndex += 1;
				break;
			}
			case Scope::ValveV:
			{
				if (token == "[")
					break;

				if (token == "]")
				{
					setScope(Scope::Rotation);
					break;
				}

				setCurrentVectorComponentForValveUV(token, componentIndex.value(), UV::V);
				*componentIndex += 1;
				break;
			}
			case Scope::Rotation:
			{
				currentFace.uvExtra.rot = std::stof(std::string(token));
				setScope(Scope::UScale);
				break;
			}
			case Scope::UScale:
			{
				currentFace.uvExtra.scaleX = std::stof(std::string(token));
				setScope(Scope::VScale);
				break;
			}
			case Scope::VScale:
			{
				currentFace.uvExtra.scaleY = std::stof(std::string(token));
				submitCurrentFaceToCurrentBrush();
				setScope(Scope::Brush);
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

		generateAllGeometry(mapData);

		return {};
	}
}