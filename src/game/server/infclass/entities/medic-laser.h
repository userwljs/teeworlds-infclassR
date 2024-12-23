#ifndef GAME_SERVER_ENTITIES_MEDIC_LASER_H
#define GAME_SERVER_ENTITIES_MEDIC_LASER_H

#include "infc-laser.h"

class CMedicLaser : public CInfClassLaser
{
public:
	static void OnFired(CIcCharacter *pCharacter, WeaponFireContext *pFireContext, float StartEnergy);

	CMedicLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, EInfclassWeapon Weapon);

protected:
	bool OnCharacterHit(CIcCharacter *pHit) final;

};

#endif // GAME_SERVER_ENTITIES_MEDIC_LASER_H
