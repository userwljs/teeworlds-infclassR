/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_INFCLASS_ENTITIES_LASER_H
#define GAME_SERVER_INFCLASS_ENTITIES_LASER_H

#include "ic_entity.h"

struct WeaponFireContext;
enum class EDamageType;
enum class EInfclassWeapon;

class CInfClassLaser : public CIcEntity
{
public:
	CInfClassLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Dmg, EInfclassWeapon InfClassWeapon);

	static void MakeLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Dmg, EInfclassWeapon InfClassWeapon);

	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	virtual void DoBounce();

	void SetExplosive(bool Explosive);

protected:
	EDamageType GetDamageType() const;

	virtual bool HitCharacter(vec2 From, vec2 To);
	virtual bool OnCharacterHit(CInfClassCharacter *pHit);

protected:
	vec2 m_From;
	vec2 m_Dir;
	const EInfclassWeapon m_Weapon;
	float m_Energy;
	int m_Bounces = 0;
	int m_MaxBounces = 0;
	int m_BounceCost = 0;
	int m_EvalTick = 0;
	int m_Dmg = 0;
	bool m_Explosive = false;
};

#endif // GAME_SERVER_INFCLASS_ENTITIES_LASER_H
