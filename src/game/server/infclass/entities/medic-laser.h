#ifndef GAME_SERVER_ENTITIES_MEDIC_LASER_H
#define GAME_SERVER_ENTITIES_MEDIC_LASER_H

#include "ic_laser.h"

class CMedicLaser : public CIcLaser
{
public:
	static void OnFired(CIcCharacter *pCharacter, WeaponFireContext *pFireContext, float StartEnergy);

	CMedicLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, EInfclassWeapon Weapon);

protected:
	bool OnCharacterHit(CIcCharacter *pHit, const vec2 &At) final;

};

#endif // GAME_SERVER_ENTITIES_MEDIC_LASER_H
