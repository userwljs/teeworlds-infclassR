/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_ENGINEER_WALL_H
#define GAME_SERVER_ENTITIES_ENGINEER_WALL_H

#include <game/server/infclass/entities/ic_placed_object.h>

class CIcCharacter;

class CEngineerWall : public CPlacedObject
{
public:
	static int EntityId;

	CEngineerWall(CGameContext *pGameContext, vec2 Pos, int Owner);
	~CEngineerWall() override;

	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;
	void OnHitInfected(CIcCharacter *pCharacter);
	void OnSurvivalHitInfected(CIcCharacter *pCharacter);

private:
	void PrepareSnapData();

	int m_EndPointId{};
	int m_WallFlashTicks{};
	int m_SnapStartTick{};
	int m_PlayerNextDamageTick[MAX_CLIENTS] = {0};
};

#endif
