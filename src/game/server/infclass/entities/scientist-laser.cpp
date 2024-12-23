/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "scientist-laser.h"

#include <game/infclass/damage_type.h>
#include <game/infclass/weapons.h>
#include <game/server/infclass/classes/humans/human.h>
#include <game/server/infclass/ic_gamecontroller.h>

#include "growingexplosion.h"
#include "ic_character.h"
#include "white-hole.h"

CScientistLaser::CScientistLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Dmg)
	: CInfClassLaser(pGameContext, Pos, Direction, StartEnergy, Owner, Dmg, EInfclassWeapon::EXPLOSIVE_LASER)
{
	DoBounce();
}

bool CScientistLaser::OnCharacterHit(CIcCharacter *pHit)
{
	return true;
}

void CScientistLaser::DoBounce()
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

	GameController()->CreateExplosion(m_Pos, GetOwner(), EDamageType::SCIENTIST_LASER);
	CreateWhiteHole(GetPos(), To);
}

void CScientistLaser::CreateWhiteHole(const vec2 &CenterPos, const vec2 &To)
{
	// Create a white hole entity
	CIcCharacter *pOwnerChar = GetOwnerCharacter();
	CInfClassHuman *pHuman = CInfClassHuman::GetInstance(pOwnerChar->GetPlayer());

	if(!pHuman || !pHuman->HasWhiteHole())
	{
		return;
	}
	new CGrowingExplosion(GameServer(), CenterPos, vec2(0.0, -1.0), GetOwner(), 5, EDamageType::WHITE_HOLE);
	new CWhiteHole(GameServer(), To, GetOwner());

	// Make it unavailable
	pHuman->RemoveWhiteHole();
}
