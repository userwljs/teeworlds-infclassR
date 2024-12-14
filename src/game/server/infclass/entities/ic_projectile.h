#pragma once

#include <game/server/infclass/entities/infcentity.h>

enum class TAKEDAMAGEMODE;
enum class EDamageType;

class CProjectile : public CInfCEntity
{
public:
	static int EntityId;

	CProjectile(CGameContext *pGameContext, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
		int Damage, bool Explosive, float Force, int SoundImpact, EDamageType DamageType);

	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	void FlashGrenade();
	void SetFlashRadius(int Radius);

private:
	vec2 m_Direction;
	int m_LifeSpan;
	int m_Type;
	int m_Damage;
	int m_SoundImpact;
	int m_Weapon;
	EDamageType m_DamageType;
	float m_Force;
	int m_StartTick;
	bool m_Explosive;
	
	bool m_IsFlashGrenade;
	int m_FlashRadius = 0;
	vec2 m_StartPos;
	TAKEDAMAGEMODE m_TakeDamageMode;

};
