/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "merc-laser.h"

#include <game/server/gamecontext.h>

#include <engine/shared/config.h>

#include <game/server/infclass/classes/humans/human.h>
#include <game/server/infclass/entities/merc-bomb.h>
#include <game/server/infclass/ic_gamecontroller.h>

static constexpr int MercLaserDamage = 0;

CMercenaryLaser::CMercenaryLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, float UpgradePoints) : CIcLaser(pGameContext, Pos, Direction, StartEnergy, Owner, MercLaserDamage, EInfclassWeapon::MERCENARY_UPGRADE_LASER), m_UpgradePoints(UpgradePoints)
{
	CIcLaser::DoBounce();
}

bool CMercenaryLaser::HitTarget(vec2 From, vec2 To)
{
	CMercenaryBomb *pCurrentBomb = nullptr;
	for(TEntityPtr<CMercenaryBomb> pBomb = GameWorld()->FindFirst<CMercenaryBomb>(); pBomb; ++pBomb)
	{
		if(pBomb->GetOwner() == GetOwner())
		{
			pCurrentBomb = pBomb;
			break;
		}
	}

	if(pCurrentBomb == nullptr)
		return false;

	vec2 IntersectPos;
	if(closest_point_on_line(From, To, pCurrentBomb->GetPos(), IntersectPos))
	{
		float Len = distance(pCurrentBomb->GetPos(), IntersectPos);
		if(Len < pCurrentBomb->GetLaserHitRadius())
		{
			CInfClassHuman *pMercClass = CInfClassHuman::GetInstance(GetOwnerCharacter());
			pMercClass->UpgradeMercBomb(pCurrentBomb, m_UpgradePoints);

			m_From = From;
			m_Pos = IntersectPos;
			m_Energy = -1;
			return true;
		}
	}

	return false;
}

void CMercenaryLaser::DoBounce()
{
	GameWorld()->DestroyEntity(this);
}
