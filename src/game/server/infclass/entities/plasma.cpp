// Strongly modified version of ddnet Plasma. Source: Shereef Marzouk
#include "plasma.h"

#include <engine/server.h>
#include <engine/config.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#include <game/infclass/damage_type.h>
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/ic_gamecontroller.h>

int CPlasma::EntityId{};

CPlasma::CPlasma(CGameContext *pGameContext, vec2 Pos, int Owner, int TrackedPlayer, vec2 Direction, bool Freeze, bool Explosive)
	: CIcEntity(pGameContext, EntityId, Pos, Owner)
{
	m_Freeze = Freeze;
	m_DamageType = EDamageType::NO_DAMAGE;
	m_TrackedPlayer = TrackedPlayer;
	m_Dir = Direction;
	m_Explosive = Explosive;
	m_StartTick = Server()->Tick();
	m_InitialAmount = 1.0f;

	SetLifespan(Config()->m_InfTurretPlasmaLifeSpan);
	GameWorld()->InsertEntity(this);
}

void CPlasma::Tick()
{
	CIcEntity::Tick();

	if(IsMarkedForDestroy())
		return;
	
	// tracking, position and collision calculation
	CIcCharacter *pTarget = GameController()->GetCharacter(m_TrackedPlayer);
	if(pTarget)
	{
		float Dist = distance(GetPos(), pTarget->GetPos());
		if(Dist < 24.0f)
		{
			//freeze or explode
			if (m_Freeze) 
			{
				pTarget->Freeze(3.0f, GetOwner(), FREEZEREASON_FLASH);
			}
			
			Explode();
		}
		else
		{
			m_Dir = normalize(pTarget->GetPos() - GetPos());
			m_Speed = clamp(Dist, 0.0f, 16.0f) * (1.0f - m_InitialAmount);
			m_Pos += m_Dir*m_Speed;
			
			m_InitialAmount *= 0.98f;
			
			//collision detection
			if(GameServer()->Collision()->CheckPoint(m_Pos.x, m_Pos.y)) // this only works as long as the projectile is not moving too fast
			{
				Explode();
			}
		}
	} 
	else //Target died before impact -> explode
	{
		Explode();
	}
	
}

void CPlasma::SetDamageType(EDamageType Type)
{
	m_DamageType = Type;
}

void CPlasma::Explode() 
{
	//GameServer()->CreateSound(CurPos, m_SoundImpact);
	if (m_Explosive) 
	{
		GameController()->CreateExplosion(m_Pos, GetOwner(), m_DamageType, Config()->m_InfTurretDmgFactor*0.1f);
	}
	Reset();
}

void CPlasma::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	CSnapContext Context(SnappingClientVersion);
	GameServer()->SnapLaserObject(Context, GetId(), m_Pos, m_Pos, m_StartTick, GetOwner());
}
