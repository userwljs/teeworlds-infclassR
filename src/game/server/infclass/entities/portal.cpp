/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "portal.h"

#include <game/server/infclass/entities/growingexplosion.h>
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/entities/laser-teleport.h>
#include <game/server/infclass/ic_gamecontroller.h>
#include <game/server/infclass/ic_player.h>

#include <game/server/gamecontext.h>

#include <engine/shared/config.h>

int CPortal::EntityId{};

static const float s_PortalRadius = 30.f;

void CPortal::OnPortalGunFired(CIcCharacter *pCharacter, WeaponFireContext *pFireContext)
{
	if(pFireContext->NoAmmo)
		return;

	const std::optional<vec2> PortalPos = CLaserTeleport::FindPortalPosition(pCharacter);
	if(!PortalPos.has_value())
	{
		pFireContext->FireAccepted = false;
		return;
	}

	const int OwnerCid = pCharacter->GetCid();
	vec2 OldPos = pCharacter->GetPos();
	pCharacter->SetPosition(PortalPos.value());
	pCharacter->ResetHook();
	pCharacter->GameWorld()->ReleaseHooked(OwnerCid);

	CPortal *pIn = new CPortal(pCharacter->GameServer(), OldPos, OwnerCid, CPortal::In);
	CPortal *pOut = new CPortal(pCharacter->GameServer(), PortalPos.value(), OwnerCid, CPortal::Out);

	pIn->ConnectPortal(pOut);
	pCharacter->GameServer()->CreateSound(pIn->GetPos(), pIn->GetNewEntitySound());
}

CPortal::CPortal(CGameContext *pGameContext, vec2 CenterPos, int Owner, PortalType Type) :
	CIcEntity(pGameContext, EntityId, CenterPos, Owner, s_PortalRadius)
{
	m_StartTick = Server()->Tick();
	m_EndTick = m_StartTick + Config()->m_InfSciPortalLifespan / 10.f * Server()->TickSpeed();
	m_Radius = s_PortalRadius;
	m_PortalType = Type;

	for(int i = 0; i < NUM_IDS; i++)
	{
		m_Ids[i] = Server()->SnapNewId();
	}

	StartMeridiansVisualEffect();

	GameWorld()->InsertEntity(this);
}

CPortal::~CPortal()
{
	for(int i = 0; i < NUM_IDS; i++)
	{
		Server()->SnapFreeId(m_Ids[i]);
	}

	Disconnect();
}

void CPortal::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

int CPortal::GetNewEntitySound() const
{
	return SOUND_LASER_FIRE;
}

CPortal::PortalType CPortal::GetPortalType() const
{
	return m_PortalType;
}

void CPortal::ConnectPortal(CPortal *anotherPortal)
{
	if(m_AnotherPortal == anotherPortal)
	{
		return;
	}

	if(anotherPortal)
	{
		if(anotherPortal->m_PortalType == m_PortalType)
		{
			// Invalid call
			return;
		}
		anotherPortal->m_AnotherPortal = this;
		anotherPortal->m_ConnectedTick = Server()->Tick();
		m_ConnectedTick = Server()->Tick();
	}
	m_AnotherPortal = anotherPortal;
}

void CPortal::Disconnect()
{
	if(!m_AnotherPortal)
		return;

	m_AnotherPortal->m_AnotherPortal = nullptr;
	m_AnotherPortal = nullptr;
}

CPortal *CPortal::GetAnotherPortal() const
{
	return m_AnotherPortal;
}

void CPortal::Explode()
{
	int ExplosionRadius = 4;
	new CGrowingExplosion(GameServer(), m_Pos, vec2(0.0, -1.0), GetOwner(), ExplosionRadius, EGrowingExplosionEffect::SHOCK_INFECTED);
	MarkForDestroy();
}

void CPortal::StartParallelsVisualEffect()
{
	MoveParallelsParticles();
}

void CPortal::StartMeridiansVisualEffect()
{
	float Radius = 30;
	float RandomRadius, RandomAngle;
	float VecX, VecY;
	for(int i = 0; i < NUM_HINT; i++)
	{
		RandomRadius = random_float() * (Radius - 4.0f);
		RandomAngle = 2.0f * pi * random_float();
		VecX = cos(RandomAngle);
		VecY = sin(RandomAngle);
		m_ParticlePos[i] = m_Pos + vec2(RandomRadius * VecX, RandomRadius * VecY);
		m_ParticleVec[i] = vec2(-VecX, -VecY);
	}
	// find out how long it takes for a particle to reach the mid
	RandomRadius = random_float() * (Radius - 4.0f);
	RandomAngle = 2.0f * pi * random_float();
	VecX = cos(RandomAngle);
	VecY = sin(RandomAngle);
	vec2 ParticlePos = m_Pos + vec2(Radius * VecX, Radius * VecY);
	vec2 ParticleVec = vec2(-VecX, -VecY);
	vec2 VecMid;
	float Speed;
	int i = 0;
	for(; i < 500; i++)
	{
		VecMid = m_Pos - ParticlePos;
		Speed = m_ParticleStartSpeed * clamp(1.0f - length(VecMid) / Radius + 0.5f, 0.0f, 1.0f);
		ParticlePos += vec2(ParticleVec.x * Speed, ParticleVec.y * Speed);
		if(dot(VecMid, ParticleVec) <= 0)
			break;
		ParticleVec *= m_ParticleAcceleration;
	}
	m_ParticleStopTickTime = i;
}

void CPortal::Snap(int SnappingClient)
{
	vec2 *ParticlePos = nullptr;

	// Draw AntiPing white hole effect
	vec2 AntipingParticlePos[NUM_IDS];
	static_assert(sizeof(AntipingParticlePos) == sizeof(m_ParticlePos), "Default and antiping versions must have the same SnapIDs count");

	const CIcPlayer *pPlayer = GameController()->GetPlayer(SnappingClient);
	const bool AntiPing = pPlayer && pPlayer->GetAntiPingEnabled();
	if(AntiPing)
	{
		// Draw medians

		static_assert(NUM_HINT == 12, "The antiping drawing code is hardcoded for 12 IDs");
		PrepareAntipingParticles(AntipingParticlePos);
		ParticlePos = AntipingParticlePos;
	}
	else
	{
		// AntiPing is off, draw precomputed particles
		ParticlePos = m_ParticlePos;
	}

	for(int i = 0; i < NUM_IDS; i++)
	{
		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_Ids[i], sizeof(CNetObj_Projectile)));
		if(pObj)
		{
			pObj->m_X = ParticlePos[i].x;
			pObj->m_Y = ParticlePos[i].y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = Server()->Tick();
			pObj->m_Type = WEAPON_HAMMER;
		}
	}
}

void CPortal::MoveParallelsParticles()
{
	static const float AngleStep = 2.0f * pi / NUM_SIDE;
	for(int i = 0; i < NUM_SIDE; i++)
	{
		vec2 PosStart = m_Pos + vec2(m_Radius * cos(m_Angle + AngleStep * i), m_Radius * sin(m_Angle + AngleStep * i));
		m_ParticlePos[NUM_HINT + i] = PosStart;
	}

	float AngleDelta = AngleStep / 20;
	const int readyTick = m_ConnectedTick;
	if(!m_AnotherPortal || (Server()->Tick() < readyTick))
		AngleDelta *= 0.25;

	switch(m_PortalType)
	{
	case PortalType::Disconnected:
		break;
	case PortalType::In:
		m_Angle += AngleDelta;
		break;
	case PortalType::Out:
		m_Angle -= AngleDelta;
		break;
	}

	if(m_Angle > 2.0f * pi)
	{
		m_Angle = 0;
	}
}

void CPortal::MoveMeridiansParticles()
{
	float Radius = 50;
	float MaxOutRadius = 80;
	float RandomAngle, Speed;
	float VecX, VecY;
	for(int i = 0; i < NUM_HINT; i++)
	{
		const vec2 ParticleRelativePos = m_Pos - m_ParticlePos[i];

		switch(GetPortalType())
		{
		case PortalType::Disconnected:
			if(dot(ParticleRelativePos, m_ParticleVec[i]) > 0)
			{
				Speed = m_ParticleStartSpeed * clamp(1.0f - length(ParticleRelativePos) / Radius + 0.5f, 0.0f, 1.0f);
				m_ParticlePos[i] += vec2(m_ParticleVec[i].x * Speed, m_ParticleVec[i].y * Speed);
			}
			break;
		case PortalType::In:
			Speed = m_ParticleStartSpeed * clamp(1.0f - length(ParticleRelativePos) / Radius + 0.5f, 0.0f, 1.0f);
			m_ParticlePos[i] += vec2(m_ParticleVec[i].x * Speed, m_ParticleVec[i].y * Speed);
			if(dot(ParticleRelativePos, m_ParticleVec[i]) <= 0)
			{
				RandomAngle = 2.0f * pi * random_float();
				VecX = cos(RandomAngle);
				VecY = sin(RandomAngle);
				m_ParticlePos[i] = m_Pos + vec2(Radius * VecX, Radius * VecY);
				m_ParticleVec[i] = vec2(-VecX, -VecY);
				continue;
			}
			break;
		case PortalType::Out:
			Speed = m_ParticleStartSpeed * clamp(length(ParticleRelativePos) / Radius + 0.5f, 0.0f, 1.0f);
			m_ParticlePos[i] += vec2(m_ParticleVec[i].x * Speed, m_ParticleVec[i].y * Speed);
			if(length(ParticleRelativePos) > MaxOutRadius)
			{
				RandomAngle = 2.0f * pi * random_float();
				VecX = cos(RandomAngle);
				VecY = sin(RandomAngle);
				m_ParticlePos[i] = m_Pos;
				m_ParticleVec[i] = vec2(VecX, VecY);
				continue;
			}
			break;
		}

		m_ParticleVec[i] *= m_ParticleAcceleration;
	}
}

void CPortal::TeleportCharacters()
{
	if(m_PortalType != PortalType::In)
		return;

	if(!m_AnotherPortal)
		return;

	const vec2 TargetPos = m_AnotherPortal->m_Pos;
	for(TEntityPtr<CIcCharacter> pCharacter = GameWorld()->FindFirst<CIcCharacter>(); pCharacter; ++pCharacter)
	{
		if(pCharacter->IsInfected())
			continue;

		const int CharacterClientId = pCharacter->GetCid();
		if(m_Teleported.Contains(CharacterClientId))
			continue;

		const float Distance = distance(pCharacter->m_Pos, m_Pos);
		if(Distance > pCharacter->m_ProximityRadius + m_Radius)
			continue;

		// Teleport the character
		pCharacter->SetPosition(TargetPos);
		pCharacter->ResetHook();
		GameWorld()->ReleaseHooked(CharacterClientId);

		GameServer()->CreateDeath(GetPos(), pCharacter->GetCid());
		GameServer()->CreateDeath(TargetPos, pCharacter->GetCid());

		GameServer()->CreateSound(GetPos(), SOUND_CTF_RETURN);
		m_Teleported.Add(CharacterClientId);
	}
}

void CPortal::PrepareAntipingParticles(vec2 *ParticlePos)
{
	const vec2 BasePosition = m_Pos;
	const float Scale = m_Radius / 2;
	const vec2 ArrowOffset = vec2(m_Radius * 0.25, 0);
	switch(GetPortalType())
	{
	case PortalType::Disconnected:
		for(int i = 0; i < NUM_HINT; i++)
		{
			ParticlePos[i] = m_Pos;
		}
		break;
	case PortalType::In:
		ParticlePos[0] = BasePosition + ArrowOffset + vec2(0.00, 0) * Scale;
		ParticlePos[1] = BasePosition + ArrowOffset + vec2(0.25, +0.3) * Scale;
		ParticlePos[2] = BasePosition + ArrowOffset + vec2(0.25, -0.3) * Scale;
		ParticlePos[3] = BasePosition + ArrowOffset + vec2(0.50, 0) * Scale;
		ParticlePos[4] = BasePosition + ArrowOffset + vec2(0.75, 0) * Scale;
		ParticlePos[5] = BasePosition + ArrowOffset + vec2(1.00, 0) * Scale;

		ParticlePos[6] = BasePosition - ArrowOffset + vec2(-0.00, 0.0) * Scale;
		ParticlePos[7] = BasePosition - ArrowOffset + vec2(-0.25, +0.3) * Scale;
		ParticlePos[8] = BasePosition - ArrowOffset + vec2(-0.25, -0.3) * Scale;
		ParticlePos[9] = BasePosition - ArrowOffset + vec2(-0.50, 0.0) * Scale;
		ParticlePos[10] = BasePosition - ArrowOffset + vec2(-0.75, 0.0) * Scale;
		ParticlePos[11] = BasePosition - ArrowOffset + vec2(-1.00, 0.0) * Scale;
		break;
	case PortalType::Out:
		ParticlePos[0] = BasePosition + ArrowOffset + vec2(1.00, 0) * Scale;
		ParticlePos[1] = BasePosition + ArrowOffset + vec2(0.75, +0.3) * Scale;
		ParticlePos[2] = BasePosition + ArrowOffset + vec2(0.75, -0.3) * Scale;
		ParticlePos[3] = BasePosition + ArrowOffset + vec2(0.50, 0) * Scale;
		ParticlePos[4] = BasePosition + ArrowOffset + vec2(0.25, 0) * Scale;
		ParticlePos[5] = BasePosition + ArrowOffset + vec2(0.00, 0) * Scale;

		ParticlePos[6] = BasePosition - ArrowOffset + vec2(-1.00, 0.0) * Scale;
		ParticlePos[7] = BasePosition - ArrowOffset + vec2(-0.75, +0.3) * Scale;
		ParticlePos[8] = BasePosition - ArrowOffset + vec2(-0.75, -0.3) * Scale;
		ParticlePos[9] = BasePosition - ArrowOffset + vec2(-0.50, 0.0) * Scale;
		ParticlePos[10] = BasePosition - ArrowOffset + vec2(-0.25, 0.0) * Scale;
		ParticlePos[11] = BasePosition - ArrowOffset + vec2(-0.00, 0.0) * Scale;
		break;
	}

	const int readyTick = m_ConnectedTick;
	if(!m_AnotherPortal || (Server()->Tick() < readyTick))
	{
		ParticlePos[5] = ParticlePos[0];
		ParticlePos[11] = ParticlePos[6];
	}

	// Draw parallels
	static const float AngleStep = 2.0f * pi / NUM_SIDE;
	for(int i = 0; i < NUM_SIDE; i++)
	{
		vec2 PosStart = m_Pos + vec2(m_Radius * cos(AngleStep * i), m_Radius * sin(AngleStep * i));
		ParticlePos[NUM_HINT + i] = PosStart;
	}
}

void CPortal::Tick()
{
	CIcEntity::Tick();

	if(IsMarkedForDestroy())
		return;

	// MoveParallelsParticles();
	MoveMeridiansParticles();
	TeleportCharacters();

	if(Server()->Tick() >= m_EndTick)
	{
		Explode();
	}
}

void CPortal::TickPaused()
{
	++m_StartTick;
	++m_EndTick;
}
