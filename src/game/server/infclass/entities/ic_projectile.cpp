/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/vmath.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#include <game/infclass/damage_type.h>
#include <game/server/infclass/entities/growingexplosion.h>
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/ic_gamecontroller.h>

#include "ic_projectile.h"

int CIcProjectile::EntityId{};

CIcProjectile::CIcProjectile(CGameContext *pGameContext, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
	int Damage, float Force, EDamageType DamageType) :
	CIcEntity(pGameContext, EntityId, Pos, Owner)
{
	m_Type = Type;
	m_Direction = Dir;
	m_LifeSpan = Span;
	m_Force = Force;
	m_Damage = Damage;
	m_DamageType = DamageType;
	m_StartTick = Server()->Tick();

	GameWorld()->InsertEntity(this);
	
/* INFECTION MODIFICATION START ***************************************/
	m_StartPos = Pos;
	m_Weapon = DamageTypeToWeapon(DamageType, &m_TakeDamageMode);
/* INFECTION MODIFICATION END *****************************************/
}

CIcProjectile *CIcProjectile::MakeGrenade(CGameContext *pGameContext, vec2 Pos, vec2 Direction, int Owner, EDamageType DamageType)
{
	float Force = 0;
	CIcProjectile *pProj = new CIcProjectile(pGameContext, WEAPON_GRENADE,
		Owner,
		Pos,
		Direction,
		(int)(pGameContext->Server()->TickSpeed() * pGameContext->Tuning()->m_GrenadeLifetime),
		1, Force, DamageType);

	pProj->SetExplosive(true);
	pProj->SetSoundImpact(SOUND_GRENADE_EXPLODE);

	return pProj;
}

vec2 CIcProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
		case WEAPON_GRENADE:
			Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
			Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
			Speed = GameServer()->Tuning()->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			Curvature = GameServer()->Tuning()->m_GunCurvature;
			Speed = GameServer()->Tuning()->m_GunSpeed;
			break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}


void CIcProjectile::Tick()
{
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, &CurPos, 0);
	const float ProjectileRadius = 6.0f;
	const CIcCharacter *pOwnerChar = GetOwnerCharacter();
	const bool IsInfected = pOwnerChar && pOwnerChar->IsInfected();
	CharacterFilter OnlyOtherTeamFilter = IsInfected ? CIcCharacter::GetHumansFilter() : CIcCharacter::GetInfectedFilter();
	CIcCharacter *TargetChr = CIcCharacter::GetInstance(GameWorld()->IntersectCharacter(PrevPos, CurPos, ProjectileRadius, CurPos, OnlyOtherTeamFilter));

	m_LifeSpan--;
	
/* INFECTION MODIFICATION START ***************************************/
	if(TargetChr || Collide || m_LifeSpan < 0 || GameLayerClipped(CurPos))
	{
		if(m_LifeSpan >= 0 || (m_Weapon == WEAPON_GRENADE))
		{
			if(m_SoundImpact.has_value())
				GameServer()->CreateSound(CurPos, m_SoundImpact.value());
		}

		if(m_FlashRadius)
		{
			vec2 Dir = normalize(PrevPos - CurPos);
			if(length(Dir) > 1.1) Dir = normalize(m_StartPos - CurPos);
			
			new CGrowingExplosion(GameServer(), CurPos, Dir, GetOwner(), m_FlashRadius, m_DamageType);
		}
		else if(m_Explosive)
		{
			GameController()->CreateExplosion(CurPos, GetOwner(), m_DamageType);
		}
		else if(TargetChr)
		{
			if(pOwnerChar)
			{
				if(pOwnerChar->IsHuman() && TargetChr->IsHuman())
				{
					TargetChr->TakeDamage(m_Direction * 0.001f, m_Damage, GetOwner(), m_DamageType);
				}
				else
				{
					TargetChr->TakeDamage(m_Direction * maximum(0.001f, m_Force), m_Damage, GetOwner(),m_DamageType);
				}
			}
		}

		GameWorld()->DestroyEntity(this);
	}

/* INFECTION MODIFICATION END *****************************************/
}

void CIcProjectile::TickPaused()
{
	++m_StartTick;
}

void CIcProjectile::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x*100.0f);
	pProj->m_VelY = (int)(m_Direction.y*100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
}

void CIcProjectile::Snap(int SnappingClient)
{
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();

	if(NetworkClipped(SnappingClient, GetPos(Ct)))
		return;

	CNetObj_Projectile *pProj = Server()->SnapNewItem<CNetObj_Projectile>(GetId());
	if(pProj)
		FillInfo(pProj);
}

/* INFECTION MODIFICATION START ***************************************/
void CIcProjectile::SetFlashRadius(int Radius)
{
	m_FlashRadius = Radius;
}

void CIcProjectile::SetExplosive(bool Value)
{
	m_Explosive = Value;
}

void CIcProjectile::SetSoundImpact(std::optional<ESound> Sound)
{
	m_SoundImpact = Sound;
}

/* INFECTION MODIFICATION END *****************************************/
