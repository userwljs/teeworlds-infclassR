#ifndef ENGINE_SERVER_MAPCONVERTER_H
#define ENGINE_SERVER_MAPCONVERTER_H

#include <base/tl/array.h>
#include <engine/map.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

class CWeakSkinInfo;

class CMapConverter
{
public:
	enum
	{
		MENUCLASS_RANDOM = 0,
		MENUCLASS_ENGINEER,
		MENUCLASS_SOLDIER,
		MENUCLASS_SCIENTIST,
		MENUCLASS_BIOLOGIST,
		MENUCLASS_LOOPER,
		MENUCLASS_MEDIC,
		MENUCLASS_HERO,
		MENUCLASS_NINJA,
		MENUCLASS_MERCENARY,
		MENUCLASS_SNIPER,
		NUM_MENUCLASS,
	};

	enum
	{
		MASK_DEFENDER = 1,
		MASK_MEDIC = 2,
		MASK_HERO = 4,
		MASK_SUPPORT = 8,
		MASK_ALL = MASK_DEFENDER | MASK_MEDIC | MASK_HERO | MASK_SUPPORT,
	};

	enum
	{
		TIMESHIFT_MENUCLASS = 60,
		TIMESHIFT_MENUCLASS_MASK = NUM_MENUCLASS + 1,
	};

	static constexpr int TIMESHIFT_MULTIPLIER = 1 + TIMESHIFT_MENUCLASS + 3 * ((MASK_ALL + 1) * TIMESHIFT_MENUCLASS_MASK);

	CMapConverter(IStorage *pStorage, IEngineMap *pMap);
	~CMapConverter();

	bool Load();
	bool CreateMap(const char *pFilename);

	int GetTimeShiftUnit() const { return m_TimeShiftUnit; }

protected:
	IEngineMap *Map() { return m_pMap; }
	IStorage *Storage() { return m_pStorage; }

	// Checks that the item range [Start, Start + Num) is inside the map item array.
	bool CheckItemRange(int Start, int Num, const char *pWhat);
	// Resolves a map item type and checks that the resulting item range is inside the item array.
	bool GetItemRange(int Type, const char *pWhat, int *pStart, int *pNum);
	// Checks that the data item is at least RequiredSize bytes long.
	bool CheckDataSize(int DataIndex, int64_t RequiredSize, const char *pWhat);
	// Resolves a string stored in the map data section and adds it to the output file.
	int AddMapDataString(int DataIndex);
	// Resolves the env points item and its size in bytes. If the source map has no env points,
	// 'ppPoints' is set to nullptr and 'pSize' to 0.
	bool GetEnvPoints(CEnvPoint **ppPoints, int64_t *pSize);
	// Checks that an envelope references only points inside the env points item (byte based, so
	// it does not depend on how many records fit into the item).
	bool CheckEnvPointRange(const CMapItemEnvelope *pItem, int64_t EnvPointsSize, const char *pWhat);

	void InitQuad(CQuad *pQuad);
	void InitQuad(CQuad *pQuad, vec2 Pos, vec2 Size);

	void CreateCircle(array<CQuad> *pQuads, vec2 Pos, float Size, vec4 Color, int Env = -1, int EnvTO = 0);

	void InitState();
	void QuantizeAnimation(int Quant);

	void CopyVersion();
	void CopyMapInfo();
	bool CopyImages();
	bool CopySounds();
	bool CopyGameLayer();
	bool CopyLayers();
	bool CopyAnimations();

	void AddImageQuad(const char *pName, int ImageId, int GridX, int GridY, int X, int Y, int Width, int Height, vec2 Pos, vec2 Size, vec4 Color, int Env);
	void AddTeeLayer(const char *pName, int ImageId, vec2 Pos, float Size, int Env, bool Black, const CWeakSkinInfo &SkinInfo);
	int AddExternalImage(const char *pImageName, int Width, int Height);
	int AddEmbeddedImage(const char *pImageName, int Width, int Height, bool GrayScale);

	int Finalize();

protected:
	IStorage *m_pStorage;
	IEngineMap *m_pMap;
	CDataFileWriter m_DataFile;

	CTile *m_pTiles;
	CTile *m_pPhysicsLayerTiles;
	int m_Width;
	int m_Height;

	int m_NumGroups;
	int m_NumLayers;
	int m_NumImages;
	int m_NumSounds;
	int m_NumEnvs;
	array<CEnvPoint> m_lEnvPoints;

	vec2 m_MenuPosition;
	int m_AnimationCycle;
	int m_TimeShiftUnit;
};

#endif
