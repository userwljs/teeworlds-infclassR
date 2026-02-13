/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"

#include <engine/shared/config.h>
#include <game/animation.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
CEntity::CEntity(CGameWorld *pGameWorld, int ObjType, const vec2 &Pos, int ProximityRadius)
{
	m_pGameWorld = pGameWorld;
	m_pCCollision = GameServer()->Collision();

	m_ObjType = ObjType;
	m_Pos = Pos;
	m_ProximityRadius = ProximityRadius;

	m_MarkedForDestroy = false;
	m_Id = Server()->SnapNewId();

	m_pPrevTypeEntity = nullptr;
	m_pNextTypeEntity = nullptr;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeId(m_Id);
}

bool CEntity::NetworkClipped(int SnappingClient) const
{
	return m_pGameWorld->GameServer()->NetworkClipped(SnappingClient, m_Pos);
}

bool CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos) const
{
	return m_pGameWorld->GameServer()->NetworkClipped(SnappingClient, CheckPos);
}

bool CEntity::NetworkClippedLine(int SnappingClient, vec2 StartPos, vec2 EndPos) const
{
	return m_pGameWorld->GameServer()->NetworkClippedLine(SnappingClient, StartPos, EndPos);
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x) / 32 < -200 || round_to_int(CheckPos.x) / 32 > GameServer()->Collision()->GetWidth() + 200 ||
		   round_to_int(CheckPos.y) / 32 < -200 || round_to_int(CheckPos.y) / 32 > GameServer()->Collision()->GetHeight() + 200;
}

CAnimatedEntity::CAnimatedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pivot) :
	CEntity(pGameWorld, Objtype),
	m_Pivot(Pivot),
	m_RelPosition(vec2(0.0f, 0.0f)),
	m_PosEnv(-1)
{
}

CAnimatedEntity::CAnimatedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pivot, vec2 RelPosition, int PosEnv) :
	CEntity(pGameWorld, Objtype),
	m_Pivot(Pivot),
	m_RelPosition(RelPosition),
	m_PosEnv(PosEnv)
{
}

void CAnimatedEntity::Tick()
{
	vec2 Position(0.0f, 0.0f);
	float Angle = 0.0f;
	if(m_PosEnv >= 0)
	{
		GetAnimationTransform(GameServer()->m_pController->GetTime(), m_PosEnv, GameServer()->Layers(), Position, Angle);
	}

	float x = (m_RelPosition.x * cosf(Angle) - m_RelPosition.y * sinf(Angle));
	float y = (m_RelPosition.x * sinf(Angle) + m_RelPosition.y * cosf(Angle));

	m_Pos = Position + m_Pivot + vec2(x, y);
}
