/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_BOUNCINGBULLET_H
#define GAME_SERVER_ENTITIES_BOUNCINGBULLET_H

#include "ic_entity.h"

class CBouncingBullet : public CIcEntity
{
public:
	static int EntityId;

	CBouncingBullet(CGameContext *pGameContext, int Owner, vec2 Pos, vec2 Dir);

	vec2 GetPos(float Time) const;
	void FillInfo(CNetObj_Projectile *pProj) const;

	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

private:
	vec2 m_ActualPos;
	vec2 m_ActualDir;
	vec2 m_Direction;
	int m_StartTick;
	int m_BounceLeft;
	float m_DistanceLeft;
};

#endif
