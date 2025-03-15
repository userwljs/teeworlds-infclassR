#pragma once

#include <game/server/infclass/entities/ic_entity.h>

enum class TAKEDAMAGEMODE;
enum class EDamageType;

class CIcProjectile : public CIcEntity
{
public:
	static int EntityId;

	CIcProjectile(CGameContext *pGameContext, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
		int Damage, float Force, EDamageType DamageType);

	static CIcProjectile *MakeGrenade(CGameContext *pGameContext, vec2 Pos, vec2 Direction, int Owner, EDamageType DamageType);

	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	void SetFlashRadius(int Radius);

	void SetExplosive(bool Value);

	void SetSoundImpact(std::optional<ESound> Sound);

private:
	vec2 m_Direction;
	int m_LifeSpan;
	int m_Type;
	int m_Damage;
	std::optional<ESound> m_SoundImpact;
	int m_Weapon;
	EDamageType m_DamageType;
	float m_Force;
	int m_StartTick;
	bool m_Explosive{};

	int m_FlashRadius{};
	vec2 m_StartPos;
	TAKEDAMAGEMODE m_TakeDamageMode;

};
