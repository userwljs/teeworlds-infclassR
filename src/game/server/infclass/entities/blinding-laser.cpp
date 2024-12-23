#include "blinding-laser.h"

#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#include <game/server/infclass/ic_gamecontroller.h>

#include "ic_character.h"

void CBlindingLaser::OnFired(CIcCharacter *pCharacter, WeaponFireContext *pFireContext)
{
	if(pFireContext->NoAmmo)
		return;

	CBlindingLaser *pLaser = new CBlindingLaser(pCharacter->GameContext(), pCharacter->GetPos(), pCharacter->GetDirection(), pCharacter->GetCid());
	pCharacter->GameServer()->CreateSound(pCharacter->GetPos(), SOUND_LASER_FIRE);
	pLaser->DoBounce();
}

CBlindingLaser::CBlindingLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, int Owner)
	: CIcLaser(pGameContext, Pos, Direction, 600, Owner, 0, EInfclassWeapon::BLINDING_LASER)
{
}

bool CBlindingLaser::OnCharacterHit(CIcCharacter *pHit)
{
	pHit->MakeBlind(Config()->m_InfBlindnessDuration, GetOwner());
	return true;
}

void CBlindingLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	vec2 To = m_Pos + m_Dir * m_Energy;

	if(GameServer()->Collision()->IntersectLineWeapon(m_Pos, To, 0x0, &To))
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}
	else
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}
}
