#include <engine/server/mapconverter.h>

#include <base/color.h>
#include <base/io.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/gfx/image_loader.h>
#include <engine/graphics.h>

#include <game/server/infclass/events-director.h>

#include <game/server/entities/character.h>

#include <game/collision.h>
#include <game/infclass/ic_classes.h>
#include <game/layers.h>
#include <game/server/infclass/classes/humans/human.h>
#include <game/server/infclass/ic_gamecontroller.h>
#include <game/server/teeinfo.h>

#include <cstddef>
#include <limits>

// Copies a map item into an output struct. Older maps contain shorter item versions, so only the
// bytes that are present in the source are copied and the remaining fields stay zeroed. The
// destination does not have to be pre-initialized.
template<typename T>
static void CopyMapItemPrefix(T *pDst, const void *pSrc, int SrcSize)
{
	mem_zero(pDst, sizeof(T));
	if(SrcSize > 0)
		mem_copy(pDst, pSrc, minimum(SrcSize, static_cast<int>(sizeof(T))));
}

class CClientGameTileGetter
{
public:
	explicit CClientGameTileGetter(CCollision *pCollision);

	int GetClientGameTileIndex(int TileX, int TileY) const;

protected:
	CCollision *m_pCollision = nullptr;
	int m_ZoneHandle_icDamage = -1;
	int m_ZoneHandle_icBonus = -1;
};

CClientGameTileGetter::CClientGameTileGetter(CCollision *pCollision) :
	m_pCollision(pCollision)
{
	m_ZoneHandle_icDamage = m_pCollision->GetZoneHandle("icDamage");
	m_ZoneHandle_icBonus = m_pCollision->GetZoneHandle("icBonus");
}

int CClientGameTileGetter::GetClientGameTileIndex(int TileX, int TileY) const
{
	int W = m_pCollision->GetWidth();
	int GameLayerIndex = m_pCollision->GetTileIndex(TileY * W + TileX);
	switch(GameLayerIndex)
	{
	case TILE_SOLID:
	case TILE_NOHOOK:
	case TILE_START:
	case TILE_FINISH:
	case TILE_ENTITIES_OFF_1:
	case TILE_ENTITIES_OFF_2:
		return GameLayerIndex;
	default:
		break;
	}

	float X = TileX * 32 + 16;
	float Y = TileY * 32 + 16;

	int icDamageIndex = m_pCollision->GetZoneValueAt(m_ZoneHandle_icDamage, X, Y);

	switch(icDamageIndex)
	{
	case ZONE_DAMAGE_DEATH:
		return TILE_DEATH;
	case ZONE_DAMAGE_DEATH_NOUNDEAD:
		return TILE_HIT_DISABLE;
	case ZONE_DAMAGE_DEATH_INFECTED:
		return TILE_DUNFREEZE;
	case ZONE_DAMAGE_INFECTION:
		return TILE_TELEINWEAPON;
	default:
		break;
	}

	int icBonusIndex = m_pCollision->GetZoneValueAt(m_ZoneHandle_icBonus, X, Y);
	switch(icBonusIndex)
	{
	case ZONE_BONUS_BONUS:
		return TILE_THROUGH_CUT;
	default:
		break;
	}

	return TILE_AIR;
}

int LoadPNG(CImageInfo *pImg, const char *pFilename)
{
	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(File)
	{
		io_seek(File, 0, EIoSeekOrigin::END);
		unsigned int FileSize = io_tell(File);
		io_seek(File, 0, EIoSeekOrigin::START);

		TImageByteBuffer ByteBuffer;
		SImageByteBuffer ImageByteBuffer(&ByteBuffer);

		ByteBuffer.resize(FileSize);
		io_read(File, &ByteBuffer.front(), FileSize);

		io_close(File);

		uint8_t *pImgBuffer = nullptr;
		EImageFormat ImageFormat;
		int PngliteIncompatible;
		if(::LoadPNG(ImageByteBuffer, pFilename, PngliteIncompatible, pImg->m_Width, pImg->m_Height, pImgBuffer, ImageFormat))
		{
			pImg->m_pData = pImgBuffer;

			if(ImageFormat == IMAGE_FORMAT_RGB) // ignore_convention
				pImg->m_Format = CImageInfo::FORMAT_RGB;
			else if(ImageFormat == IMAGE_FORMAT_RGBA) // ignore_convention
				pImg->m_Format = CImageInfo::FORMAT_RGBA;
			else
			{
				free(pImgBuffer);
				return 0;
			}

			if(PngliteIncompatible != 0)
			{
				dbg_msg("game/png", "\"%s\" is not compatible with pnglite and cannot be loaded by old DDNet versions: ", pFilename);
			}
		}
		else
		{
			dbg_msg("game/png", "image had unsupported image format. filename='%s'", pFilename);
			return 0;
		}
	}
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	return 1;
}

void FreePNG(CImageInfo *pImg)
{
	free(pImg->m_pData);
	pImg->m_pData = nullptr;
}

void MakeGrayScale(CImageInfo *pImg)
{
	static const int BodySize = 96;
	// From CSkins::LoadSkin()
	// game/client/components/skins.cpp:216
	unsigned char *d = static_cast<unsigned char *>(pImg->m_pData);
	int Pitch = pImg->m_Width * 4;

	// create colorless version
	int Step = pImg->m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;

	// make the texture gray scale
	for(int i = 0; i < pImg->m_Width * pImg->m_Height; i++)
	{
		int v = (d[i * Step] + d[i * Step + 1] + d[i * Step + 2]) / 3;
		d[i * Step] = v;
		d[i * Step + 1] = v;
		d[i * Step + 2] = v;
	}

	int Freq[256] = {0};
	int OrgWeight = 0;
	int NewWeight = 192;

	// The body analysis only looks at the top left corner of the skin. Images smaller
	// than that (or non-RGBA data) must not be read out of bounds.
	if(pImg->m_Format != CImageInfo::FORMAT_RGBA || pImg->m_Width < BodySize || pImg->m_Height < BodySize)
		return;

	// find most common frequence
	for(int y = 0; y < BodySize; y++)
		for(int x = 0; x < BodySize; x++)
		{
			if(d[y * Pitch + x * 4 + 3] > 128)
				Freq[d[y * Pitch + x * 4]]++;
		}

	for(int i = 1; i < 256; i++)
	{
		if(Freq[OrgWeight] < Freq[i])
			OrgWeight = i;
	}

	// reorder
	int InvOrgWeight = 255 - OrgWeight;
	int InvNewWeight = 255 - NewWeight;
	for(int y = 0; y < BodySize; y++)
	{
		for(int x = 0; x < BodySize; x++)
		{
			int v = d[y * Pitch + x * 4];
			if(v <= OrgWeight && OrgWeight == 0)
				v = 0;
			else if(v <= OrgWeight)
				v = static_cast<int>((v / static_cast<float>(OrgWeight)) * NewWeight);
			else if(InvOrgWeight == 0)
				v = NewWeight;
			else
				v = static_cast<int>(((v - OrgWeight) / static_cast<float>(InvOrgWeight)) * InvNewWeight + NewWeight);
			d[y * Pitch + x * 4] = v;
			d[y * Pitch + x * 4 + 1] = v;
			d[y * Pitch + x * 4 + 2] = v;
		}
	}
}

void SetQuadColor(CQuad *Quad, int Color)
{
	ColorRGBA BodyColor = color_cast<ColorRGBA>(ColorHSLA(Color).UnclampLighting(ColorHSLA::DARKEST_LGT));
	CColor TypedColor;
	TypedColor.r = BodyColor.r * 255;
	TypedColor.g = BodyColor.g * 255;
	TypedColor.b = BodyColor.b * 255;
	TypedColor.a = BodyColor.a * 255;
	Quad->m_aColors[0] = TypedColor;
	Quad->m_aColors[1] = TypedColor;
	Quad->m_aColors[2] = TypedColor;
	Quad->m_aColors[3] = TypedColor;
}

CMapConverter::CMapConverter(IStorage *pStorage, IEngineMap *pMap) :
	m_pStorage(pStorage),
	m_pMap(pMap),
	m_pTiles(nullptr)
{
}

CMapConverter::~CMapConverter()
{
	if(m_pTiles)
		delete[] m_pTiles;
}

// Work with 'long long' to properly handle overflows (later)
static inline long long gcd_long(long long a, long long b)
{
	while(b != 0)
	{
		long long c = a % b;
		a = b;
		b = c;
	}
	return a;
}

// Sizes of the fields the converter reads from map items. Map item layouts have changed over
// time and older maps contain shorter items, so every field access is checked against these.
static constexpr int GROUP_LAYERS_SIZE = sizeof(CMapItemGroup_v1);
static constexpr int TILEMAP_FLAGS_SIZE = offsetof(CMapItemLayerTilemap, m_Flags) + sizeof(int);
static constexpr int TILEMAP_DATA_SIZE = offsetof(CMapItemLayerTilemap, m_Data) + sizeof(int);
static constexpr int TILEMAP_TELE_SIZE = offsetof(CMapItemLayerTilemap, m_Tele) + sizeof(int);
static constexpr int QUADS_DATA_SIZE = offsetof(CMapItemLayerQuads, m_Data) + sizeof(int);
static constexpr int SOUNDS_DATA_SIZE = offsetof(CMapItemLayerSounds, m_Data) + sizeof(int);

// The map format stores an envelope point as 6 ints (m_Time, m_Curvetype and 4 values).
static constexpr int ENV_POINT_SIZE = 6 * static_cast<int>(sizeof(int));
static_assert(sizeof(CEnvPoint) == ENV_POINT_SIZE, "the map format stores CEnvPoint as 6 ints");

// Checks that an item is at least MinSize bytes long
static bool CheckItemSize(IMap *pMap, int Index, int MinSize, const char *pWhat)
{
	const int ItemSize = pMap->GetItemSize(Index);
	if(ItemSize < MinSize)
	{
		log_error("MapConverter", "%s has an invalid item size: %d bytes (at least %d expected)", pWhat, ItemSize, MinSize);
		return false;
	}
	return true;
}

// Returns the item at Index after checking its size. The index must have been validated against
// the item array before, e.g. by GetItemRange().
template<typename T>
static T *GetCheckedItem(IMap *pMap, int Index, int MinSize, const char *pWhat)
{
	if(!CheckItemSize(pMap, Index, MinSize, pWhat))
		return nullptr;

	T *pItem = static_cast<T *>(pMap->GetItem(Index, nullptr, nullptr));
	if(!pItem)
		log_error("MapConverter", "failed to read %s", pWhat);
	return pItem;
}

bool CMapConverter::CheckItemRange(int Start, int Num, const char *pWhat)
{
	const int NumItems = Map()->NumItems();
	if(Start < 0 || Num < 0 || Start > NumItems || Num > NumItems - Start)
	{
		log_error("MapConverter", "invalid %s item range: start=%d num=%d (map has %d items)", pWhat, Start, Num, NumItems);
		return false;
	}
	return true;
}

bool CMapConverter::GetItemRange(int Type, const char *pWhat, int *pStart, int *pNum)
{
	Map()->GetType(Type, pStart, pNum);
	return CheckItemRange(*pStart, *pNum, pWhat);
}

bool CMapConverter::CheckDataSize(int DataIndex, int64_t RequiredSize, const char *pWhat)
{
	if(RequiredSize < 0 || RequiredSize > std::numeric_limits<int>::max())
	{
		log_error("MapConverter", "%s is too large: %lld bytes", pWhat, static_cast<long long>(RequiredSize));
		return false;
	}

	const int DataSize = Map()->GetDataSize(DataIndex);
	if(DataSize < 0 || static_cast<int64_t>(DataSize) < RequiredSize)
	{
		log_error("MapConverter", "%s is truncated: data index=%d has %d bytes, %lld required", pWhat, DataIndex, DataSize, static_cast<long long>(RequiredSize));
		return false;
	}
	return true;
}

int CMapConverter::AddMapDataString(int DataIndex)
{
	if(DataIndex < 0)
		return -1;

	const int Size = Map()->GetDataSize(DataIndex);
	if(Size <= 0)
		return -1;

	const char *pData = static_cast<const char *>(Map()->GetData(DataIndex));
	if(pData == nullptr || pData[Size - 1] != '\0' || mem_has_null(pData, Size - 1) || !str_utf8_check(pData))
		return -1;

	return m_DataFile.AddDataString(pData);
}

bool CMapConverter::GetEnvPoints(CEnvPoint **ppPoints, int64_t *pSize)
{
	*ppPoints = nullptr;
	*pSize = 0;

	int Start, Num;
	if(!GetItemRange(MAPITEMTYPE_ENVPOINTS, "env points", &Start, &Num))
		return false;

	if(Num <= 0)
		return true;

	const int Size = Map()->GetItemSize(Start);
	if(Size <= 0)
		return true;

	*ppPoints = GetCheckedItem<CEnvPoint>(Map(), Start, 0, "env points");
	if(!*ppPoints)
		return false;

	*pSize = Size;
	return true;
}

bool CMapConverter::CheckEnvPointRange(const CMapItemEnvelope *pItem, int64_t EnvPointsSize, const char *pWhat)
{
	const int64_t RequiredSize = (static_cast<int64_t>(pItem->m_StartPoint) + pItem->m_NumPoints) * ENV_POINT_SIZE;
	if(pItem->m_NumPoints < 0 || pItem->m_StartPoint < 0 || RequiredSize > EnvPointsSize)
	{
		log_error("MapConverter", "%s references envelope points outside of the map data: start=%d num=%d (%lld/%lld bytes)",
			pWhat, pItem->m_StartPoint, pItem->m_NumPoints, static_cast<long long>(RequiredSize), static_cast<long long>(EnvPointsSize));
		return false;
	}
	return true;
}

bool CMapConverter::Load()
{
	m_AnimationCycle = 1;
	m_MenuPosition = vec2(0.0f, 0.0f);

	// Find GameLayer
	int LayersStart, LayersNum, GroupsStart, GroupsNum;
	if(!GetItemRange(MAPITEMTYPE_GROUP, "groups", &GroupsStart, &GroupsNum) ||
		!GetItemRange(MAPITEMTYPE_LAYER, "layers", &LayersStart, &LayersNum))
		return false;

	CMapItemLayerTilemap *pPhysicsLayer = nullptr;

	// Find the gamelayer
	for(int g = 0; g < GroupsNum; g++)
	{
		char aGroup[64];
		str_format(aGroup, sizeof(aGroup), "group %d", g);

		CMapItemGroup *pGroup = GetCheckedItem<CMapItemGroup>(Map(), GroupsStart + g, GROUP_LAYERS_SIZE, aGroup);
		if(!pGroup)
			return false;
		if(pGroup->m_NumLayers < 0 || pGroup->m_StartLayer < 0 ||
			static_cast<int64_t>(pGroup->m_StartLayer) + pGroup->m_NumLayers > LayersNum)
		{
			log_error("MapConverter", "%s references an invalid layer range: start=%d num=%d (map has %d layers)",
				aGroup, pGroup->m_StartLayer, pGroup->m_NumLayers, LayersNum);
			return false;
		}

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			const int LayerIndex = LayersStart + pGroup->m_StartLayer + l;

			char aLayer[64];
			str_format(aLayer, sizeof(aLayer), "layer %d of group %d", l, g);

			CMapItemLayer *pLayer = GetCheckedItem<CMapItemLayer>(Map(), LayerIndex, sizeof(CMapItemLayer), aLayer);
			if(!pLayer)
				return false;

			if(pLayer->m_Type != LAYERTYPE_TILES)
				continue;

			CMapItemLayerTilemap *pTileLayer = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
			if(!CheckItemSize(Map(), LayerIndex, TILEMAP_FLAGS_SIZE, aLayer))
				return false;

			if(pTileLayer->m_Flags & TILESLAYERFLAG_GAME)
			{
				if(!CheckItemSize(Map(), LayerIndex, TILEMAP_DATA_SIZE, "the game layer"))
					return false;

				pPhysicsLayer = pTileLayer;
				break;
			}
		}
		if(pPhysicsLayer)
			break;
	}

	if(!pPhysicsLayer)
	{
		log_error("MapConverter", "no physics layer in loaded map");
		return false;
	}

	m_Width = pPhysicsLayer->m_Width;
	m_Height = pPhysicsLayer->m_Height;

	if(m_Width <= 0 || m_Height <= 0)
	{
		log_error("MapConverter", "invalid game layer size: %dx%d", m_Width, m_Height);
		return false;
	}

	const int64_t TileCount = static_cast<int64_t>(m_Width) * m_Height;
	if(TileCount > static_cast<int64_t>(std::numeric_limits<int>::max()) / static_cast<int64_t>(sizeof(CTile)))
	{
		log_error("MapConverter", "game layer is too large: %dx%d", m_Width, m_Height);
		return false;
	}

	if(!CheckDataSize(pPhysicsLayer->m_Data, TileCount * static_cast<int64_t>(sizeof(CTile)), "game layer tiles"))
		return false;

	m_pPhysicsLayerTiles = static_cast<CTile *>(m_pMap->GetData(pPhysicsLayer->m_Data));
	if(!m_pPhysicsLayerTiles)
	{
		log_error("MapConverter", "failed to read the game layer tiles");
		return false;
	}

	if(m_pTiles)
		delete[] m_pTiles;

	m_pTiles = new CTile[static_cast<size_t>(TileCount)]();

	for(int j = 0; j < m_Height; j++)
	{
		for(int i = 0; i < m_Width; i++)
		{
			m_pTiles[j * m_Width + i].m_Flags = 0;
			m_pTiles[j * m_Width + i].m_Reserved = 0;

			int Skip = m_pPhysicsLayerTiles[j * m_Width + i].m_Skip;
			m_pTiles[j * m_Width + i].m_Skip = Skip;
			i += Skip;
		}
	}

	// Get the animation cycle
	CEnvPoint *pEnvPoints = nullptr;
	int64_t EnvPointsSize = 0;
	if(!GetEnvPoints(&pEnvPoints, &EnvPointsSize))
		return false;

	const int MenuItemSyncMagic = TIMESHIFT_MENUCLASS * 1000;
	long long AnimationCircle = m_AnimationCycle;
	long long TimeShiftUnit = 0;
	bool TimeShiftError = false;

	if(pEnvPoints)
	{
		static const int Quant = 100;
		QuantizeAnimation(Quant);
	}

	// Scan the envelopes even without env points: maps without any envelope keep the default
	// animation circle, which is what the menu time shift unit is derived from.
	{
		int Start, Num;
		if(!GetItemRange(MAPITEMTYPE_ENVELOPE, "envelopes", &Start, &Num))
			return false;

		for(int i = 0; i < Num; i++)
		{
			char aEnvelope[64];
			str_format(aEnvelope, sizeof(aEnvelope), "envelope %d", i);

			CMapItemEnvelope *pItem = GetCheckedItem<CMapItemEnvelope>(Map(), Start + i, sizeof(CMapItemEnvelope_v1), aEnvelope);
			if(!pItem)
				return false;
			if(!CheckEnvPointRange(pItem, EnvPointsSize, aEnvelope))
				return false;

			if(pItem->m_NumPoints > 0 && pEnvPoints)
			{
				// Read the ends as 64 bit: subtracting two malformed 'int' times may overflow.
				const long long FirstTime = pEnvPoints[pItem->m_StartPoint].m_Time;
				const long long LastTime = pEnvPoints[pItem->m_StartPoint + pItem->m_NumPoints - 1].m_Time;
				const long long Duration = LastTime - FirstTime;
				if(Duration < 0)
				{
					log_warn("MapConverter", "%s has unsorted envelope points (duration %lld ms), ignoring it",
						aEnvelope, Duration);
				}
				else if(Duration > 0)
				{
					dbg_msg("MapConverter", "Duration found: %lld", Duration);

					// lcm(a, b) = (a / gcd) * b. Compare to avoid overflowing.
					const long long Gcd = gcd_long(AnimationCircle, Duration);
					if(AnimationCircle / Gcd > std::numeric_limits<int>::max() / TIMESHIFT_MULTIPLIER / Duration)
					{
						log_warn("MapConverter", "%s: the animation cycle is too long for the class menu (%lld ms and %lld ms)",
							aEnvelope, AnimationCircle, Duration);
						TimeShiftError = true;
						break;
					}

					AnimationCircle = (AnimationCircle / Gcd) * Duration;
					dbg_msg("MapConverter", "New AnimationCycle: %lld", AnimationCircle);
				}
			}
		}
	}

	dbg_msg("MapConverter", "Final AnimationCycle: %lld", AnimationCircle);
	if(TimeShiftUnit == 0)
	{
		if(AnimationCircle)
			TimeShiftUnit = AnimationCircle * ((MenuItemSyncMagic / AnimationCircle) + 1);
		else
			TimeShiftUnit = MenuItemSyncMagic;
	}

	long long MaxUsedAnimationTime = TIMESHIFT_MULTIPLIER * TimeShiftUnit;
	long long MaxAvailable = std::numeric_limits<int>::max();

	if(!TimeShiftError)
	{
		if(MaxUsedAnimationTime > MaxAvailable)
		{
			log_warn("MapConverter", "The animation duration is off the limit: %lld/%lld", MaxUsedAnimationTime, MaxAvailable);
			TimeShiftUnit = 0;
		}
		else
		{
			log_info("MapConverter", "The animation duration is within the limits: %lld/%lld", MaxUsedAnimationTime, MaxAvailable);
		}
	}
	m_TimeShiftUnit = TimeShiftError ? 0 : static_cast<int>(TimeShiftUnit);

	return true;
}

void CMapConverter::InitQuad(CQuad *pQuad)
{
	for(auto &Point : pQuad->m_aPoints)
	{
		Point.x = 0;
		Point.y = 0;
	}
	pQuad->m_aColors[0].r = pQuad->m_aColors[1].r = 255;
	pQuad->m_aColors[0].g = pQuad->m_aColors[1].g = 255;
	pQuad->m_aColors[0].b = pQuad->m_aColors[1].b = 255;
	pQuad->m_aColors[0].a = pQuad->m_aColors[1].a = 255;
	pQuad->m_aColors[2].r = pQuad->m_aColors[3].r = 255;
	pQuad->m_aColors[2].g = pQuad->m_aColors[3].g = 255;
	pQuad->m_aColors[2].b = pQuad->m_aColors[3].b = 255;
	pQuad->m_aColors[2].a = pQuad->m_aColors[3].a = 255;
	pQuad->m_aTexcoords[0].x = 0;
	pQuad->m_aTexcoords[0].y = 0;
	pQuad->m_aTexcoords[1].x = 1 << 10;
	pQuad->m_aTexcoords[1].y = 0;
	pQuad->m_aTexcoords[2].x = 0;
	pQuad->m_aTexcoords[2].y = 1 << 10;
	pQuad->m_aTexcoords[3].x = 1 << 10;
	pQuad->m_aTexcoords[3].y = 1 << 10;
	pQuad->m_PosEnv = -1;
	pQuad->m_PosEnvOffset = 0;
	pQuad->m_ColorEnv = -1;
	pQuad->m_ColorEnvOffset = 0;
}

void CMapConverter::InitQuad(CQuad *pQuad, vec2 Pos, vec2 Size)
{
	int X0 = f2fx(Pos.x - Size.x / 2.0f);
	int X1 = f2fx(Pos.x + Size.x / 2.0f);
	int XC = f2fx(Pos.x);
	int Y0 = f2fx(Pos.y - Size.y / 2.0f);
	int Y1 = f2fx(Pos.y + Size.y / 2.0f);
	int YC = f2fx(Pos.y);

	InitQuad(pQuad);
	pQuad->m_aPoints[0].x = pQuad->m_aPoints[2].x = X0;
	pQuad->m_aPoints[1].x = pQuad->m_aPoints[3].x = X1;
	pQuad->m_aPoints[0].y = pQuad->m_aPoints[1].y = Y0;
	pQuad->m_aPoints[2].y = pQuad->m_aPoints[3].y = Y1;
	pQuad->m_aPoints[4].x = XC;
	pQuad->m_aPoints[4].y = YC;
}

void CMapConverter::CreateCircle(array<CQuad> *pQuads, vec2 CenterPos, float Size, vec4 Color, int Env, int EnvTO)
{
	CQuad Quad;
	InitQuad(&Quad);
	Quad.m_aPoints[0].x = f2fx(CenterPos.x);
	Quad.m_aPoints[0].y = f2fx(CenterPos.y);
	Quad.m_aColors[0].r = Quad.m_aColors[1].r = Quad.m_aColors[2].r = Quad.m_aColors[3].r = Color.r * 255.0f;
	Quad.m_aColors[0].g = Quad.m_aColors[1].g = Quad.m_aColors[2].g = Quad.m_aColors[3].g = Color.g * 255.0f;
	Quad.m_aColors[0].b = Quad.m_aColors[1].b = Quad.m_aColors[2].b = Quad.m_aColors[3].b = Color.b * 255.0f;
	Quad.m_aColors[0].a = Quad.m_aColors[1].a = Quad.m_aColors[2].a = Quad.m_aColors[3].a = Color.a * 255.0f;

	Quad.m_ColorEnv = Env;
	Quad.m_ColorEnvOffset = EnvTO;

	float AngleStep = 360.0f / 32.0f;
	float AngleIter = AngleStep;
	vec2 RadiusVect = vec2(Size / 2.0f, 0.0f);
	vec2 LastPos = CenterPos + RadiusVect;
	for(int i = 0; i < 32; i++)
	{
		vec2 PosMid = CenterPos + rotate(RadiusVect, AngleIter - AngleStep / 2.0f);
		vec2 Pos = CenterPos + rotate(RadiusVect, AngleIter);

		Quad.m_aPoints[1].x = f2fx(LastPos.x);
		Quad.m_aPoints[1].y = f2fx(LastPos.y);
		Quad.m_aPoints[2].x = f2fx(Pos.x);
		Quad.m_aPoints[2].y = f2fx(Pos.y);
		Quad.m_aPoints[3].x = f2fx(PosMid.x);
		Quad.m_aPoints[3].y = f2fx(PosMid.y);
		Quad.m_aPoints[4].x = f2fx((PosMid.x + CenterPos.x) / 2.0f);
		Quad.m_aPoints[4].y = f2fx((PosMid.y + CenterPos.y) / 2.0f);

		pQuads->add(Quad);

		LastPos = Pos;
		AngleIter += AngleStep;
	}
}

void CMapConverter::AddImageQuad(const char *pName, int ImageId, int GridX, int GridY, int X, int Y, int Width, int Height, vec2 Pos, vec2 Size, vec4 Color, int Env)
{
	array<CQuad> aQuads;
	CQuad Quad;

	float StepX = 1.0f / GridX;
	float StepY = 1.0f / GridY;

	InitQuad(&Quad, Pos, Size);
	Quad.m_ColorEnv = Env;
	Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = f2fx(StepX * X);
	Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = f2fx(StepX * (X + Width));
	Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = f2fx(StepY * Y);
	Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = f2fx(StepY * (Y + Height));
	Quad.m_aColors[0].r = Quad.m_aColors[1].r = Quad.m_aColors[2].r = Quad.m_aColors[3].r = Color.r * 255.0f;
	Quad.m_aColors[0].g = Quad.m_aColors[1].g = Quad.m_aColors[2].g = Quad.m_aColors[3].g = Color.g * 255.0f;
	Quad.m_aColors[0].b = Quad.m_aColors[1].b = Quad.m_aColors[2].b = Quad.m_aColors[3].b = Color.b * 255.0f;
	Quad.m_aColors[0].a = Quad.m_aColors[1].a = Quad.m_aColors[2].a = Quad.m_aColors[3].a = Color.a * 255.0f;
	aQuads.add(Quad);

	CMapItemLayerQuads Item{};
	Item.m_Version = 2;
	Item.m_Layer.m_Flags = 0;
	Item.m_Layer.m_Type = LAYERTYPE_QUADS;
	Item.m_Image = ImageId;
	Item.m_NumQuads = aQuads.size();
	StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pName);
	Item.m_Data = m_DataFile.AddDataSwapped(aQuads.size() * sizeof(CQuad), aQuads.base_ptr());

	m_DataFile.AddItem(MAPITEMTYPE_LAYER, m_NumLayers++, sizeof(Item), &Item);
}

void CMapConverter::AddTeeLayer(const char *pName, int ImageId, vec2 Pos, float Size, int Env, bool Black, const CWeakSkinInfo &SkinInfo)
{
	array<CQuad> aQuads;
	CQuad Quad;

	// Body, Shadow
	InitQuad(&Quad, Pos + vec2(0.0f, -4.0f), vec2(Size, Size));
	Quad.m_ColorEnv = Env;
	Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = f2fx(96.0f / 256.0f);
	Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = f2fx(192.0f / 256.0f);
	Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = 0;
	Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = f2fx(96.0f / 128.0f);
	aQuads.add(Quad);

	// BackFeet, Shadow
	InitQuad(&Quad, Pos + vec2(-7.0f, 10.0f), vec2(Size, Size / 2.0f));
	Quad.m_ColorEnv = Env;
	Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = 1024.0f * 192.0f / 256.0f;
	Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = 1024.0f;
	Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = 1024.0f * 64.0f / 128.0f;
	Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = 1024.0f * 96.0f / 128.0f;
	aQuads.add(Quad);

	// FrontFeet, Shadow
	InitQuad(&Quad, Pos + vec2(7.0f, 10.0f), vec2(Size, Size / 2.0f));
	Quad.m_ColorEnv = Env;
	Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = 1024.0f * 192.0f / 256.0f;
	Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = 1024.0f;
	Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = 1024.0f * 64.0f / 128.0f;
	Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = 1024.0f * 96.0f / 128.0f;
	aQuads.add(Quad);

	// BackFeet, Color
	if(!Black)
	{
		InitQuad(&Quad, Pos + vec2(-7.0f, 10.0f), vec2(Size, Size / 2.0f));
		Quad.m_ColorEnv = Env;
		Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = 1024.0f * 192.0f / 256.0f;
		Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = 1024.0f;
		Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = 1024.0f * 32.0f / 128.0f;
		Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = 1024.0f * 64.0f / 128.0f;
		aQuads.add(Quad);

		// Body, Color
		InitQuad(&Quad, Pos + vec2(0.0f, -4.0f), vec2(Size, Size));
		Quad.m_ColorEnv = Env;
		Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = 0;
		Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = 1024.0f * 96.0f / 256.0f;
		Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = 0;
		Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = 1024.0f * 96.0f / 128.0f;
		if(SkinInfo.UseCustomColor)
		{
			SetQuadColor(&Quad, SkinInfo.ColorBody);
		}
		aQuads.add(Quad);

		// FrontFeet, Color
		InitQuad(&Quad, Pos + vec2(7.0f, 10.0f), vec2(Size, Size / 2.0f));
		Quad.m_ColorEnv = Env;
		Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = 1024.0f * 192.0f / 256.0f;
		Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = 1024.0f;
		Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = 1024.0f * 32.0f / 128.0f;
		Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = 1024.0f * 64.0f / 128.0f;
		if(SkinInfo.UseCustomColor)
		{
			SetQuadColor(&Quad, SkinInfo.ColorFeet);
		}
		aQuads.add(Quad);

		// Eyes
		vec2 Direction = normalize(vec2(1.0f, -0.5f));
		float EyeSeparation = (0.075f - 0.010f * absolute(Direction.x)) * Size;
		vec2 Offset = vec2(Direction.x * 0.125f, -0.05f + Direction.y * 0.10f) * Size;

		// Left
		InitQuad(&Quad, Pos + vec2(Offset.x - EyeSeparation, Offset.y - 4.0f), vec2(Size * 0.40f, Size * 0.40f));
		Quad.m_ColorEnv = Env;
		Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = 1024.0f * 64.0f / 256.0f;
		Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = 1024.0f * 96.0f / 256.0f;
		Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = 1024.0f * 96.0f / 128.0f;
		Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = 1024.0f * 128.0f / 128.0f;
		aQuads.add(Quad);

		// Right
		InitQuad(&Quad, Pos + vec2(Offset.x + EyeSeparation, Offset.y - 4.0f), vec2(-Size * 0.40f, Size * 0.40f));
		Quad.m_ColorEnv = Env;
		Quad.m_aTexcoords[0].x = Quad.m_aTexcoords[2].x = 1024.0f * 64.0f / 256.0f;
		Quad.m_aTexcoords[1].x = Quad.m_aTexcoords[3].x = 1024.0f * 96.0f / 256.0f;
		Quad.m_aTexcoords[0].y = Quad.m_aTexcoords[1].y = 1024.0f * 96.0f / 128.0f;
		Quad.m_aTexcoords[2].y = Quad.m_aTexcoords[3].y = 1024.0f * 128.0f / 128.0f;
		aQuads.add(Quad);
	}

	CMapItemLayerQuads Item{};
	Item.m_Version = Item.m_Layer.m_Version = 2;
	Item.m_Layer.m_Flags = 0;
	Item.m_Layer.m_Type = LAYERTYPE_QUADS;
	Item.m_Image = ImageId;
	Item.m_NumQuads = aQuads.size();
	StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pName);
	Item.m_Data = m_DataFile.AddDataSwapped(aQuads.size() * sizeof(CQuad), aQuads.base_ptr());

	m_DataFile.AddItem(MAPITEMTYPE_LAYER, m_NumLayers++, sizeof(Item), &Item);
}

void CMapConverter::InitState()
{
	m_NumGroups = 0;
	m_NumLayers = 0;
	m_NumImages = 0;
	m_NumSounds = 0;
	m_NumEnvs = 0;
	m_lEnvPoints.clear();
}

void CMapConverter::QuantizeAnimation(int Quant)
{
	// Get the animation cycle
	CEnvPoint *pEnvPoints = nullptr;
	int64_t EnvPointsSize = 0;
	if(!GetEnvPoints(&pEnvPoints, &EnvPointsSize) || !pEnvPoints)
		return;

	int Start, Num;
	if(!GetItemRange(MAPITEMTYPE_ENVELOPE, "envelopes", &Start, &Num))
		return;

	for(int i = 0; i < Num; i++)
	{
		char aEnvelope[64];
		str_format(aEnvelope, sizeof(aEnvelope), "envelope %d", i);

		CMapItemEnvelope *pItem = GetCheckedItem<CMapItemEnvelope>(Map(), Start + i, sizeof(CMapItemEnvelope_v1), aEnvelope);
		if(!pItem)
			return;
		if(!CheckEnvPointRange(pItem, EnvPointsSize, aEnvelope))
			return;

		if(pItem->m_NumPoints > 0)
		{
			// Quant the points:
			for(int p = 0; p < pItem->m_NumPoints; ++p)
			{
				int PointOffest = pItem->m_StartPoint + p;
				int Time = pEnvPoints[PointOffest].m_Time;
				int S = Time / Quant;
				if((Time % Quant) >= Quant / 2)
				{
					++S;
				}
				int QuantedTime = S * Quant;
				if(Time != QuantedTime)
				{
					pEnvPoints[PointOffest].m_Time = QuantedTime;
				}
			}
		}
	}
}

void CMapConverter::CopyVersion()
{
	CMapItemVersion *pItem = static_cast<CMapItemVersion *>(Map()->FindItem(MAPITEMTYPE_VERSION, 0));
	if(!pItem)
	{
		CMapItemVersion Version{};
		Version.m_Version = 1;
		m_DataFile.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(Version), &Version);
		return;
	}

	m_DataFile.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(CMapItemVersion), pItem);
}

void CMapConverter::CopyMapInfo()
{
	CMapItemInfo *pSourceItem = static_cast<CMapItemInfo *>(Map()->FindItem(MAPITEMTYPE_INFO, 0));
	if(!pSourceItem)
	{
		// The source map has no info item, keep the generated map consistent with it.
		return;
	}

	CMapItemInfo Item{};
	Item.m_Version = 1;
	Item.m_Author = AddMapDataString(pSourceItem->m_Author);
	Item.m_MapVersion = AddMapDataString(pSourceItem->m_MapVersion);
	Item.m_Credits = AddMapDataString(pSourceItem->m_Credits);
	Item.m_License = AddMapDataString(pSourceItem->m_License);

	m_DataFile.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Item), &Item);
}

bool CMapConverter::CopyImages()
{
	int Start, Num;
	if(!GetItemRange(MAPITEMTYPE_IMAGE, "images", &Start, &Num))
		return false;

	for(int i = 0; i < Num; i++)
	{
		char aImage[64];
		str_format(aImage, sizeof(aImage), "image %d", i);

		CMapItemImage *pItem = GetCheckedItem<CMapItemImage>(Map(), Start + i, sizeof(CMapItemImage), aImage);
		if(!pItem)
			return false;

		CMapItemImage ImageItem{};
		ImageItem.m_Version = 1;
		ImageItem.m_Width = pItem->m_Width;
		ImageItem.m_Height = pItem->m_Height;
		ImageItem.m_External = pItem->m_External;
		ImageItem.m_ImageName = AddMapDataString(pItem->m_ImageName);
		if(pItem->m_External)
		{
			ImageItem.m_ImageData = -1;
		}
		else
		{
			if(ImageItem.m_Width <= 0 || ImageItem.m_Height <= 0)
			{
				log_error("MapConverter", "%s has an invalid size: %dx%d", aImage, ImageItem.m_Width, ImageItem.m_Height);
				return false;
			}

			const int64_t ImageDataSize = static_cast<int64_t>(ImageItem.m_Width) * ImageItem.m_Height * 4;
			if(!CheckDataSize(pItem->m_ImageData, ImageDataSize, "image data"))
				return false;

			void *pData = Map()->GetData(pItem->m_ImageData);
			if(!pData)
			{
				log_error("MapConverter", "failed to read the data of %s", aImage);
				return false;
			}
			ImageItem.m_ImageData = m_DataFile.AddData(static_cast<size_t>(ImageDataSize), pData);
		}
		m_DataFile.AddItem(MAPITEMTYPE_IMAGE, m_NumImages++, sizeof(ImageItem), &ImageItem);

		// unload image
		Map()->UnloadData(pItem->m_ImageData);
		Map()->UnloadData(pItem->m_ImageName);
	}
	return true;
}

bool CMapConverter::CopySounds()
{
	int Start, Num;
	if(!GetItemRange(MAPITEMTYPE_SOUND, "sounds", &Start, &Num))
		return false;

	for(int i = 0; i < Num; i++)
	{
		char aSound[64];
		str_format(aSound, sizeof(aSound), "sound %d", i);

		CMapItemSound *pItem = GetCheckedItem<CMapItemSound>(Map(), Start + i, sizeof(CMapItemSound), aSound);
		if(!pItem)
			return false;

		CMapItemSound SoundItem{};
		SoundItem.m_Version = 1;
		SoundItem.m_SoundName = AddMapDataString(pItem->m_SoundName);
		SoundItem.m_External = pItem->m_External;
		if(pItem->m_External)
		{
			SoundItem.m_SoundData = -1;
		}
		else
		{
			if(pItem->m_SoundDataSize < 0)
			{
				log_error("MapConverter", "%s has an invalid size: %d", aSound, pItem->m_SoundDataSize);
				return false;
			}

			if(!CheckDataSize(pItem->m_SoundData, pItem->m_SoundDataSize, "sound data"))
				return false;

			void *pData = Map()->GetData(pItem->m_SoundData);
			if(!pData)
			{
				log_error("MapConverter", "failed to read the data of %s", aSound);
				return false;
			}
			SoundItem.m_SoundData = m_DataFile.AddData(pItem->m_SoundDataSize, pData);
			SoundItem.m_SoundDataSize = pItem->m_SoundDataSize;
		}

		m_DataFile.AddItem(MAPITEMTYPE_SOUND, m_NumSounds++, sizeof(SoundItem), &SoundItem);

		// unload sound
		Map()->UnloadData(pItem->m_SoundData);
		Map()->UnloadData(pItem->m_SoundName);
	}
	return true;
}

bool CMapConverter::CopyAnimations()
{
	CEnvPoint *pEnvPoints = nullptr;
	int64_t EnvPointsSize = 0;
	if(!GetEnvPoints(&pEnvPoints, &EnvPointsSize))
		return false;

	int Start, Num;
	if(!GetItemRange(MAPITEMTYPE_ENVELOPE, "envelopes", &Start, &Num))
		return false;

	int NumUsedEnvPoints = 0;
	for(int i = 0; i < Num; i++)
	{
		char aEnvelope[64];
		str_format(aEnvelope, sizeof(aEnvelope), "envelope %d", i);

		CMapItemEnvelope *pItem = GetCheckedItem<CMapItemEnvelope>(Map(), Start + i, sizeof(CMapItemEnvelope_v1), aEnvelope);
		if(!pItem)
			return false;
		if(!CheckEnvPointRange(pItem, EnvPointsSize, aEnvelope))
			return false;

		CMapItemEnvelope Item;
		CopyMapItemPrefix(&Item, pItem, Map()->GetItemSize(Start + i));
		m_DataFile.AddItem(MAPITEMTYPE_ENVELOPE, m_NumEnvs++, sizeof(Item), &Item);

		if(pItem->m_NumPoints > 0)
			NumUsedEnvPoints = maximum(NumUsedEnvPoints, pItem->m_StartPoint + pItem->m_NumPoints);
	}

	// NumUsedEnvPoints > 0 implies that the envelopes were checked against a non-empty env
	// points item above.
	for(int i = 0; i < NumUsedEnvPoints; i++)
	{
		m_lEnvPoints.add(pEnvPoints[i]);
	}
	return true;
}

bool CMapConverter::CopyGameLayer()
{
	CMapItemLayerTilemap Item{};
	Item.m_Version = Item.m_Layer.m_Version = 3;
	Item.m_Layer.m_Flags = 0;
	Item.m_Layer.m_Type = LAYERTYPE_TILES;
	Item.m_Color.r = 255;
	Item.m_Color.g = 255;
	Item.m_Color.b = 255;
	Item.m_Color.a = 255;
	Item.m_ColorEnv = -1;
	Item.m_ColorEnvOffset = 0;
	Item.m_Width = m_Width;
	Item.m_Height = m_Height;
	Item.m_Flags = 1;
	Item.m_Image = -1;

	CLayers Layers;
	Layers.Init(Map());
	if(!Layers.GameLayer())
	{
		log_error("MapConverter", "the source map has no game layer");
		return false;
	}

	CCollision Collision;
	Collision.Init(&Layers);

	CClientGameTileGetter TilesGetter(&Collision);

	// Cleanup the game layer
	// This will make maps no more usable by a server, but the original ones are in the repository
	for(int j = 0; j < m_Height; j++)
	{
		for(int i = 0; i < m_Width; i++)
		{
			m_pTiles[j * m_Width + i].m_Index = TilesGetter.GetClientGameTileIndex(i, j);
		}
	}

	Item.m_Data = m_DataFile.AddData(static_cast<size_t>(m_Width) * m_Height * sizeof(CTile), m_pTiles);
	StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Game");
	m_DataFile.AddItem(MAPITEMTYPE_LAYER, m_NumLayers++, sizeof(Item), &Item);
	return true;
}

bool CMapConverter::CopyLayers()
{
	int LayersStart, LayersNum, Start, Num;
	if(!GetItemRange(MAPITEMTYPE_LAYER, "layers", &LayersStart, &LayersNum) ||
		!GetItemRange(MAPITEMTYPE_GROUP, "groups", &Start, &Num))
		return false;

	// Groups
	for(int g = 0; g < Num; g++)
	{
		char aGroup[64];
		str_format(aGroup, sizeof(aGroup), "group %d", g);

		CMapItemGroup *pGItem = GetCheckedItem<CMapItemGroup>(Map(), Start + g, GROUP_LAYERS_SIZE, aGroup);
		if(!pGItem)
			return false;

		if(pGItem->m_Version != CMapItemGroup::CURRENT_VERSION)
		{
			log_error("MapConverter", "%s has an unsupported version %d (expected %d)", aGroup, pGItem->m_Version, CMapItemGroup::CURRENT_VERSION);
			return false;
		}

		if(pGItem->m_NumLayers < 0 || pGItem->m_StartLayer < 0 ||
			static_cast<int64_t>(pGItem->m_StartLayer) + pGItem->m_NumLayers > LayersNum)
		{
			log_error("MapConverter", "%s references an invalid layer range: start=%d num=%d (map has %d layers)",
				aGroup, pGItem->m_StartLayer, pGItem->m_NumLayers, LayersNum);
			return false;
		}

		CMapItemGroup GroupItem;
		CopyMapItemPrefix(&GroupItem, pGItem, Map()->GetItemSize(Start + g));
		GroupItem.m_StartLayer = m_NumLayers;

		int GroupLayers = 0;

		// Layers
		for(int l = 0; l < pGItem->m_NumLayers; l++)
		{
			const int LayerIndex = LayersStart + pGItem->m_StartLayer + l;

			char aLayer[64];
			str_format(aLayer, sizeof(aLayer), "layer %d of group %d", l, g);

			const CMapItemLayer *pLayerItem = GetCheckedItem<CMapItemLayer>(Map(), LayerIndex, sizeof(CMapItemLayer), aLayer);
			if(!pLayerItem)
				return false;

			const int LayerItemSize = Map()->GetItemSize(LayerIndex);

			if(pLayerItem->m_Type == LAYERTYPE_TILES)
			{
				const CMapItemLayerTilemap *pTilemapItem = reinterpret_cast<const CMapItemLayerTilemap *>(pLayerItem);

				// The fields used below live in the common part of every tilemap item version.
				if(!CheckItemSize(Map(), LayerIndex, TILEMAP_DATA_SIZE, aLayer))
					return false;

				if(pTilemapItem->m_Flags & TILESLAYERFLAG_GAME)
				{
					if(!CopyGameLayer())
						return false;
					GroupLayers++;
				}
				else
				{
					if(pTilemapItem->m_Width <= 0 || pTilemapItem->m_Height <= 0)
					{
						log_error("MapConverter", "%s has an invalid size: %dx%d", aLayer, pTilemapItem->m_Width, pTilemapItem->m_Height);
						return false;
					}

					const int64_t TilesSize = static_cast<int64_t>(pTilemapItem->m_Width) * pTilemapItem->m_Height * static_cast<int64_t>(sizeof(CTile));
					if(!CheckDataSize(pTilemapItem->m_Data, TilesSize, "layer tiles"))
						return false;

					void *pData = Map()->GetData(pTilemapItem->m_Data);
					if(!pData)
					{
						log_error("MapConverter", "failed to read the tiles of %s", aLayer);
						return false;
					}

					CMapItemLayerTilemap LayerItem;
					CopyMapItemPrefix(&LayerItem, pTilemapItem, LayerItemSize);

					LayerItem.m_Data = m_DataFile.AddData(static_cast<size_t>(TilesSize), pData);
					if(pTilemapItem->m_Flags & TILESLAYERFLAG_TELE)
					{
						// Older tilemap item versions have no tele data field.
						if(!CheckItemSize(Map(), LayerIndex, TILEMAP_TELE_SIZE, aLayer))
							return false;

						const int64_t TeleTilesSize = static_cast<int64_t>(pTilemapItem->m_Width) * pTilemapItem->m_Height * static_cast<int64_t>(sizeof(CTeleTile));
						if(!CheckDataSize(pTilemapItem->m_Tele, TeleTilesSize, "layer tele tiles"))
							return false;

						pData = Map()->GetData(pTilemapItem->m_Tele);
						if(!pData)
						{
							log_error("MapConverter", "failed to read the tele tiles of %s", aLayer);
							return false;
						}
						LayerItem.m_Tele = m_DataFile.AddData(static_cast<size_t>(TeleTilesSize), pData);
					}

					m_DataFile.AddItem(MAPITEMTYPE_LAYER, m_NumLayers++, sizeof(LayerItem), &LayerItem);

					Map()->UnloadData(pTilemapItem->m_Data);

					GroupLayers++;
				}
			}
			else if(pLayerItem->m_Type == LAYERTYPE_QUADS)
			{
				const CMapItemLayerQuads *pQuadsItem = reinterpret_cast<const CMapItemLayerQuads *>(pLayerItem);
				if(!CheckItemSize(Map(), LayerIndex, QUADS_DATA_SIZE, aLayer))
					return false;

				CMapItemLayerQuads LayerItem;
				CopyMapItemPrefix(&LayerItem, pQuadsItem, LayerItemSize);
				if(LayerItem.m_NumQuads != 0)
				{
					if(LayerItem.m_NumQuads < 0)
					{
						log_error("MapConverter", "%s has an invalid number of quads: %d", aLayer, LayerItem.m_NumQuads);
						return false;
					}

					const int64_t QuadsSize = static_cast<int64_t>(LayerItem.m_NumQuads) * static_cast<int64_t>(sizeof(CQuad));
					if(!CheckDataSize(pQuadsItem->m_Data, QuadsSize, "layer quads"))
						return false;

					void *pData = Map()->GetData(pQuadsItem->m_Data);
					if(!pData)
					{
						log_error("MapConverter", "failed to read the quads of %s", aLayer);
						return false;
					}
					LayerItem.m_Data = m_DataFile.AddData(static_cast<size_t>(QuadsSize), pData);
				}
				else
				{
					// add dummy data for backwards compatibility
					// this allows the layer to be loaded with an empty array since m_NumQuads is 0 while saving
					CQuad Dummy{};
					LayerItem.m_Data = m_DataFile.AddDataSwapped(sizeof(CQuad), &Dummy);
				}
				m_DataFile.AddItem(MAPITEMTYPE_LAYER, m_NumLayers++, sizeof(LayerItem), &LayerItem);

				Map()->UnloadData(pQuadsItem->m_Data);

				GroupLayers++;
			}
			else if(pLayerItem->m_Type == LAYERTYPE_SOUNDS)
			{
				const CMapItemLayerSounds *pSoundsItem = reinterpret_cast<const CMapItemLayerSounds *>(pLayerItem);
				if(!CheckItemSize(Map(), LayerIndex, SOUNDS_DATA_SIZE, aLayer))
					return false;

				if(pSoundsItem->m_NumSources < 0)
				{
					log_error("MapConverter", "%s has an invalid number of sound sources: %d", aLayer, pSoundsItem->m_NumSources);
					return false;
				}

				const int64_t SourcesSize = static_cast<int64_t>(pSoundsItem->m_NumSources) * static_cast<int64_t>(sizeof(CSoundSource));
				if(!CheckDataSize(pSoundsItem->m_Data, SourcesSize, "layer sound sources"))
					return false;

				void *pData = Map()->GetData(pSoundsItem->m_Data);
				if(!pData)
				{
					log_error("MapConverter", "failed to read the sound sources of %s", aLayer);
					return false;
				}

				CMapItemLayerSounds LayerItem;
				CopyMapItemPrefix(&LayerItem, pSoundsItem, LayerItemSize);
				LayerItem.m_Data = m_DataFile.AddData(static_cast<size_t>(SourcesSize), pData);

				m_DataFile.AddItem(MAPITEMTYPE_LAYER, m_NumLayers++, sizeof(LayerItem), &LayerItem);

				Map()->UnloadData(pSoundsItem->m_Data);

				GroupLayers++;
			}
		}

		GroupItem.m_NumLayers = GroupLayers;

		m_DataFile.AddItem(MAPITEMTYPE_GROUP, m_NumGroups++, sizeof(GroupItem), &GroupItem);
	}
	return true;
}

int CMapConverter::AddExternalImage(const char *pImageName, int Width, int Height)
{
	CMapItemImage Item{};
	Item.m_Version = 1;
	Item.m_External = 1;
	Item.m_ImageData = -1;
	Item.m_ImageName = m_DataFile.AddData(str_length(pImageName) + 1, pImageName);
	Item.m_Width = Width;
	Item.m_Height = Height;
	m_DataFile.AddItem(MAPITEMTYPE_IMAGE, m_NumImages++, sizeof(Item), &Item);

	return m_NumImages - 1;
}

int CMapConverter::AddEmbeddedImage(const char *pImageName, int Width, int Height, bool GrayScale)
{
	CImageInfo Img;
	CImageInfo *pImg = &Img;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "mapres/%s.png", pImageName);

	char aFullPath[512];
	m_pStorage->GetDataPath(aBuf, aFullPath, sizeof(aFullPath));

	if(!LoadPNG(pImg, aFullPath))
	{
		return -1;
	}

	if(pImg->m_Width <= 0 || pImg->m_Height <= 0 ||
		static_cast<int64_t>(pImg->m_Width) * pImg->m_Height * 4 > static_cast<int64_t>(std::numeric_limits<int>::max()))
	{
		dbg_msg("MapConverter", "image '%s' has an invalid or too large size: %dx%d", pImageName, pImg->m_Width, pImg->m_Height);
		FreePNG(pImg);
		return -1;
	}

	// Normalize the pixel format to RGBA: the map format and MakeGrayScale() both expect 4 bytes per pixel.
	if(pImg->m_Format == CImageInfo::FORMAT_RGB)
	{
		unsigned char *pDataRGBA = (unsigned char *)malloc(static_cast<size_t>(pImg->m_Width) * pImg->m_Height * 4);
		if(!pDataRGBA)
		{
			FreePNG(pImg);
			return -1;
		}

		const unsigned char *pDataRGB = static_cast<const unsigned char *>(pImg->m_pData);
		for(int i = 0; i < pImg->m_Width * pImg->m_Height; i++)
		{
			pDataRGBA[i * 4] = pDataRGB[i * 3];
			pDataRGBA[i * 4 + 1] = pDataRGB[i * 3 + 1];
			pDataRGBA[i * 4 + 2] = pDataRGB[i * 3 + 2];
			pDataRGBA[i * 4 + 3] = 255;
		}

		free(pImg->m_pData);
		pImg->m_pData = pDataRGBA;
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	}

	if(GrayScale)
	{
		MakeGrayScale(pImg);
	}

	CMapItemImage Item{};
	Item.m_Version = 1;
	Item.m_External = 0;
	Item.m_Width = pImg->m_Width;
	Item.m_Height = pImg->m_Height;
	Item.m_ImageName = m_DataFile.AddData(str_length(pImageName) + 1, pImageName);
	Item.m_ImageData = m_DataFile.AddData(static_cast<size_t>(Item.m_Width) * Item.m_Height * 4, pImg->m_pData);
	m_DataFile.AddItem(MAPITEMTYPE_IMAGE, m_NumImages++, sizeof(Item), &Item);

	FreePNG(pImg);

	return m_NumImages - 1;
}

int CMapConverter::Finalize()
{
	int ClassImageId[NUM_MENUCLASS] = {};

	int DDNetVersion = 14000;
	int InfClassVersion = 0;

	for(int ClassIndex = 0; ClassIndex < NUM_MENUCLASS; ++ClassIndex)
	{
		CSkinContext SkinContext;
		SkinContext.PlayerClass = CIcGameController::MenuClassToPlayerClass(ClassIndex);

		CWeakSkinInfo ClassTeeInfo;
		if(ClassIndex == MENUCLASS_RANDOM)
		{
			ClassTeeInfo.pSkinName = "warpaint";
		}
		else if(ClassIndex == MENUCLASS_NINJA)
		{
			// Special case
			ClassTeeInfo.pSkinName = "x_ninja";
		}
		else
		{
			CInfClassHuman::SetupSkin(SkinContext, &ClassTeeInfo, DDNetVersion, InfClassVersion);
		}

		EventsDirector::SetupSkin(SkinContext, &ClassTeeInfo, DDNetVersion, InfClassVersion);

		if(!ClassTeeInfo.pSkinName)
		{
			dbg_msg("MapConverter", "no skin name for menu class %d, falling back to 'default'", ClassIndex);
			ClassTeeInfo.pSkinName = "default";
		}

		char SkinPath[96];
		str_format(SkinPath, sizeof(SkinPath), "../skins/%s", ClassTeeInfo.pSkinName);

		int ImageId = 0;
		if(ClassTeeInfo.UseCustomColor)
		{
			bool GrayScale = true;
			ImageId = AddEmbeddedImage(SkinPath, 256, 128, GrayScale);
			if(ImageId < 0)
			{
				dbg_msg("MapConverter", "Unable to access a player class skin file. Make sure "
										"that the game data (including 'data/skins') is correctly installed.");
				return -1;
			}
		}
		else
		{
			ImageId = AddExternalImage(SkinPath, 256, 128);
		}

		ClassImageId[ClassIndex] = ImageId;
	}

	// Menu

	const float MenuRadius = 196.0f;
	const float MenuAngleStart = -90.0;

	// Menu envelope times are stored as 32 bit milliseconds. The multiplication with the time
	// shift unit can exceed that range, so it is computed in 64 bit and truncated explicitly to
	// keep the result well defined (the historical behavior is a truncation to 'int').
	const auto MenuTime = [this](long long Multiplier) {
		return static_cast<int>(static_cast<uint32_t>(Multiplier * static_cast<long long>(m_TimeShiftUnit)));
	};

	{
		const float MenuAngleStep = 360.0f / static_cast<float>(NUM_MENUCLASS);

		// Menu Group
		{
			CMapItemGroup Item{};
			Item.m_Version = CMapItemGroup::CURRENT_VERSION;
			Item.m_ParallaxX = 0;
			Item.m_ParallaxY = 0;
			Item.m_OffsetX = 0;
			Item.m_OffsetY = 0;
			Item.m_StartLayer = m_NumLayers;
			Item.m_NumLayers = NUM_MENUCLASS + 1; // not sure why +1 tho
			Item.m_UseClipping = 0;
			Item.m_ClipX = 0;
			Item.m_ClipY = 0;
			Item.m_ClipW = 0;
			Item.m_ClipH = 0;
			StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "#Generated");

			m_DataFile.AddItem(MAPITEMTYPE_GROUP, m_NumGroups++, sizeof(Item), &Item);
		}
		// Menu Layers
		{
			array<CQuad> aQuads;

			int HiddenValues[4];
			int NormalValues[4];
			int HighlightValues[4];

			HiddenValues[0] = 0;
			HiddenValues[1] = 0;
			HiddenValues[2] = 0;
			HiddenValues[3] = 0;

			// Two passes: one for circles, one for tees
			for(int pass = 0; pass < 2; pass++)
			{
				if(pass == 0)
				{
					NormalValues[0] = 0;
					NormalValues[1] = 0;
					NormalValues[2] = 0;
					NormalValues[3] = 500;

					HighlightValues[0] = 1000;
					HighlightValues[1] = 1000;
					HighlightValues[2] = 1000;
					HighlightValues[3] = 500;
				}
				else
				{
					NormalValues[0] = 1000;
					NormalValues[1] = 1000;
					NormalValues[2] = 1000;
					NormalValues[3] = 1000;

					HighlightValues[0] = 1000;
					HighlightValues[1] = 1000;
					HighlightValues[2] = 1000;
					HighlightValues[3] = 1000;
				}

				for(int i = 0; i < NUM_MENUCLASS; i++)
				{
					int ClassMask = 0;

					EPlayerClass PlayerClass = CIcGameController::MenuClassToPlayerClass(i);
					if(CIcGameController::IsDefenderClass(PlayerClass))
						ClassMask = MASK_DEFENDER;
					else if(CIcGameController::IsSupportClass(PlayerClass))
						ClassMask = MASK_SUPPORT;
					else if(PlayerClass == EPlayerClass::Medic)
						ClassMask = MASK_MEDIC;
					else if(PlayerClass == EPlayerClass::Hero)
						ClassMask = MASK_HERO;
					else
						ClassMask = MASK_DEFENDER; // For some reason the random class is also defender

					// Create Animation for enable/disable simulation
					{
						int StartPoint = m_lEnvPoints.size();
						int NbPoints = 0;

						{
							CEnvPoint Point;
							Point.m_Time = 0;
							Point.m_Curvetype = 0;
							Point.m_aValues[0] = HiddenValues[0];
							Point.m_aValues[1] = HiddenValues[1];
							Point.m_aValues[2] = HiddenValues[2];
							Point.m_aValues[3] = HiddenValues[3];
							m_lEnvPoints.add(Point);
							NbPoints++;
						}
						{
							CEnvPoint Point;
							Point.m_Time = MenuTime(TIMESHIFT_MENUCLASS);
							Point.m_Curvetype = 0;
							Point.m_aValues[0] = NormalValues[0];
							Point.m_aValues[1] = NormalValues[1];
							Point.m_aValues[2] = NormalValues[2];
							Point.m_aValues[3] = NormalValues[3];
							m_lEnvPoints.add(Point);
							NbPoints++;
						}

						// Iterate over all combinaisons of class availabilities
						for(int j = 0; j <= MASK_ALL; j++)
						{
							if(pass == 0 && ((ClassMask & j) || (ClassMask == -1))) // Highlight
							{
								{
									CEnvPoint Point;
									Point.m_Time = MenuTime(TIMESHIFT_MENUCLASS + 3 * ((i + 1) + j * TIMESHIFT_MENUCLASS_MASK));
									Point.m_Curvetype = 0;
									Point.m_aValues[0] = HighlightValues[0];
									Point.m_aValues[1] = HighlightValues[1];
									Point.m_aValues[2] = HighlightValues[2];
									Point.m_aValues[3] = HighlightValues[3];
									m_lEnvPoints.add(Point);
									NbPoints++;
								}
								{
									CEnvPoint Point;
									Point.m_Time = MenuTime(TIMESHIFT_MENUCLASS + 3 * ((i + 2) + j * TIMESHIFT_MENUCLASS_MASK));
									Point.m_Curvetype = 0;
									Point.m_aValues[0] = NormalValues[0];
									Point.m_aValues[1] = NormalValues[1];
									Point.m_aValues[2] = NormalValues[2];
									Point.m_aValues[3] = NormalValues[3];
									m_lEnvPoints.add(Point);
									NbPoints++;
								}
							}
							else if((ClassMask != -1) && ((ClassMask & j) == 0)) // Hide
							{
								{
									CEnvPoint Point;
									Point.m_Time = MenuTime(TIMESHIFT_MENUCLASS + 3 * (j * TIMESHIFT_MENUCLASS_MASK));
									Point.m_Curvetype = 0;
									Point.m_aValues[0] = HiddenValues[0];
									Point.m_aValues[1] = HiddenValues[1];
									Point.m_aValues[2] = HiddenValues[2];
									Point.m_aValues[3] = HiddenValues[3];
									m_lEnvPoints.add(Point);
									NbPoints++;
								}
								{
									CEnvPoint Point;
									Point.m_Time = MenuTime(TIMESHIFT_MENUCLASS + 3 * ((j + 1) * TIMESHIFT_MENUCLASS_MASK));
									Point.m_Curvetype = 0;
									Point.m_aValues[0] = NormalValues[0];
									Point.m_aValues[1] = NormalValues[1];
									Point.m_aValues[2] = NormalValues[2];
									Point.m_aValues[3] = NormalValues[3];
									m_lEnvPoints.add(Point);
									NbPoints++;
								}
							}
						}
						{
							CEnvPoint Point;
							Point.m_Time = MenuTime(TIMESHIFT_MULTIPLIER - 1);
							Point.m_Curvetype = 0;
							Point.m_aValues[0] = NormalValues[0];
							Point.m_aValues[1] = NormalValues[1];
							Point.m_aValues[2] = NormalValues[2];
							Point.m_aValues[3] = NormalValues[3];
							m_lEnvPoints.add(Point);
							NbPoints++;
						}
						{
							CEnvPoint Point;
							Point.m_Time = MenuTime(TIMESHIFT_MULTIPLIER);
							Point.m_Curvetype = 0;
							Point.m_aValues[0] = HiddenValues[0];
							Point.m_aValues[1] = HiddenValues[1];
							Point.m_aValues[2] = HiddenValues[2];
							Point.m_aValues[3] = HiddenValues[3];
							m_lEnvPoints.add(Point);
							NbPoints++;
						}

						CMapItemEnvelope Item{};
						Item.m_Version = CMapItemEnvelope::CURRENT_VERSION;
						Item.m_Channels = 4;
						Item.m_StartPoint = StartPoint;
						Item.m_NumPoints = NbPoints;
						Item.m_Synchronized = 1;
						StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Menu Class Pages");
						m_DataFile.AddItem(MAPITEMTYPE_ENVELOPE, m_NumEnvs++, sizeof(Item), &Item);
					}

					// Create Circle
					if(pass == 0)
					{
						CreateCircle(&aQuads, m_MenuPosition + rotate(vec2(MenuRadius, 0.0f), MenuAngleStart + MenuAngleStep * i), 96.0f, vec4(1.0f, 1.0f, 1.0f, 0.5f), m_NumEnvs - 1);
					}
					else
					{
						CWeakSkinInfo SkinInfo;
						vec2 Pos = m_MenuPosition + rotate(vec2(MenuRadius, 0.0f), MenuAngleStart + MenuAngleStep * i);
						if(i == MENUCLASS_RANDOM)
						{
							AddTeeLayer("Random", ClassImageId[i], Pos, 64.0f, m_NumEnvs - 1, true, SkinInfo);
						}
						else
						{
							CSkinContext SkinContext;
							SkinContext.PlayerClass = PlayerClass;
							const char *pClassName = CIcGameController::GetClassDisplayName(PlayerClass);
							CInfClassHuman::SetupSkin(SkinContext, &SkinInfo, DDNetVersion, InfClassVersion);
							EventsDirector::SetupSkin(SkinContext, &SkinInfo, DDNetVersion, InfClassVersion);
							bool Black = false;
							AddTeeLayer(pClassName, ClassImageId[i], Pos, 64.0f, m_NumEnvs - 1, Black, SkinInfo);
						}
					}
				}

				if(pass == 0)
				{
					CMapItemLayerQuads Item{};
					Item.m_Version = 2;
					Item.m_Layer.m_Version = 2;
					Item.m_Layer.m_Flags = 0;
					Item.m_Layer.m_Type = LAYERTYPE_QUADS;
					Item.m_Image = -1;
					Item.m_NumQuads = aQuads.size();
					StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "UIQuads");
					Item.m_Data = m_DataFile.AddDataSwapped(aQuads.size() * sizeof(CQuad), aQuads.base_ptr());

					m_DataFile.AddItem(MAPITEMTYPE_LAYER, m_NumLayers++, sizeof(Item), &Item);
				}
			}
		}
	}

	return 0;
}

bool CMapConverter::CreateMap(const char *pFilename)
{
	// Write to a temporary file first and rename it into place afterwards, so that a failed or
	// interrupted conversion can neither leave a partial map behind nor destroy an existing one.
	char aTmpFilename[IO_MAX_PATH_LENGTH];
	IStorage::FormatTmpPath(aTmpFilename, sizeof(aTmpFilename), pFilename);

	if(!m_DataFile.Open(Storage(), aTmpFilename))
	{
		log_error("MapConverter", "failed to open file '%s'", aTmpFilename);
		return false;
	}

	InitState();

	CopyVersion();
	CopyMapInfo();

	bool Success = CopySounds() && CopyImages() && CopyAnimations() && CopyLayers();

	if(Success && Finalize() < 0)
		Success = false;

	if(Success)
	{
		m_DataFile.AddItem(MAPITEMTYPE_ENVPOINTS, 0, m_lEnvPoints.size() * sizeof(CEnvPoint), m_lEnvPoints.base_ptr());
		Success = m_DataFile.Finish();
	}

	if(!Success)
	{
		// Close and remove the temporary file, the previously cached map (if any) stays intact.
		m_DataFile.Close();
		Storage()->RemoveFile(aTmpFilename, IStorage::TYPE_SAVE);
		log_error("MapConverter", "failed to create '%s'", pFilename);
		return false;
	}

	if(!Storage()->RenameFile(aTmpFilename, pFilename, IStorage::TYPE_SAVE))
	{
		Storage()->RemoveFile(aTmpFilename, IStorage::TYPE_SAVE);
		log_error("MapConverter", "failed to rename '%s' to '%s'", aTmpFilename, pFilename);
		return false;
	}

	log_info("MapConverter", "highres map created");
	return true;
}
