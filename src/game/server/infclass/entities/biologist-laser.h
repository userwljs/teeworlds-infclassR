/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_BIOLOGIST_LASER_H
#define GAME_SERVER_ENTITIES_BIOLOGIST_LASER_H

#include "ic_laser.h"

class CBiologistLaser final : public CIcLaser
{
public:
	CBiologistLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, int Owner, int Dmg);

protected:
	bool HitTarget(vec2 From, vec2 To) final;
};

#endif
