#pragma once

#include "ic_entity.h"

class CHealingParticle : public CIcEntity
{
public:
	static int EntityId;

	CHealingParticle(CGameContext *pGameContext, vec2 Pos, int Owner, vec2 Direction);

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	vec2 m_Direction;
};
