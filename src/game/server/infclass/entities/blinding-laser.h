#ifndef GAME_SERVER_ENTITIES_BLINDING_LASER_H
#define GAME_SERVER_ENTITIES_BLINDING_LASER_H

#include "ic_laser.h"

class CBlindingLaser : public CIcLaser
{
public:
	static void OnFired(CIcCharacter *pCharacter, WeaponFireContext *pFireContext);

	CBlindingLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, int Owner);

protected:
	bool OnCharacterHit(CIcCharacter *pHit, const vec2 &At) final;
	void DoBounce() final;

};

#endif
