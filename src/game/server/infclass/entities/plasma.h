// Strongly modified version of ddnet Plasma. Source: Shereef Marzouk
#ifndef GAME_SERVER_ENTITIES_PLASMA_H
#define GAME_SERVER_ENTITIES_PLASMA_H

#include "ic_entity.h"

enum class EDamageType;

class CPlasma : public CIcEntity
{
public:
	static int EntityId;

	CPlasma(CGameContext *pGameContext, vec2 Pos, int Owner, int TrackedPlayer, vec2 Direction, bool Freeze, bool Explosive);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetDamageType(EDamageType Type);

private:
	void Explode();

private:
	int m_StartTick;
	vec2 m_Dir;
	float m_Speed;
	int m_Freeze;
	bool m_Explosive;
	EDamageType m_DamageType;
	int m_TrackedPlayer;
	float m_InitialAmount;
};

#endif // GAME_SERVER_ENTITIES_PLASMA_H
