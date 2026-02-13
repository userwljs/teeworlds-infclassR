/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "turret.h"

#include <engine/config.h>
#include <engine/server/roundstatistics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#include <game/infclass/damage_type.h>
#include <game/server/infclass/ic_gamecontroller.h>
#include <game/server/infclass/ic_player.h>

#include "ic_character.h"
#include "ic_laser.h"
#include "plasma.h"

#include <iterator> // std::size

int CTurret::EntityId{};

CTurret::CTurret(CGameContext *pGameContext, vec2 Pos, int Owner, CTurret::Type Type) :
	CPlacedObject(pGameContext, EntityId, Pos, Owner)
{
	m_InfClassObjectType = INFCLASS_OBJECT_TYPE_TURRET;
	m_StartTick = Server()->Tick();
	m_Radius = 15.0f;
	m_foundTarget = false;
	m_ammunition = Config()->m_InfTurretAmmunition;
	m_WarmUpCounter = Server()->TickSpeed() * Config()->m_InfTurretWarmUpDuration;
	m_Type = Type;

	SetDestructable(true);

	switch(m_Type)
	{
	case LASER:
		SetReloadDuration(Config()->m_InfTurretLaserReloadDuration);
		break;
	case PLASMA:
		SetReloadDuration(Config()->m_InfTurretPlasmaReloadDuration);
		break;
	}

	for(int &Id : m_Ids)
	{
		Id = Server()->SnapNewId();
	}
	Reload();

	GameWorld()->InsertEntity(this);
}

CTurret::~CTurret()
{
	for(int SnapId : m_Ids)
	{
		Server()->SnapFreeId(SnapId);
	}
}

void CTurret::SetDestructable(bool Destructable)
{
	m_Destructable = Destructable;
}

void CTurret::Tick()
{
	CPlacedObject::Tick();

	// marked for destroy
	if(IsMarkedForDestroy())
		return;

	CIcCharacter *pKiller = nullptr;
	float ClosestLength = CCharacterCore::PhysicalSize() + HitRadius();
	ClosestLength *= ClosestLength;
	const float HitRange2 = ClosestLength;

	for(TEntityPtr<CIcCharacter> pChr = GameWorld()->FindFirst<CIcCharacter>(); pChr; ++pChr)
	{
		if(!pChr->IsInfected() || !pChr->CanDie())
			continue;

		float Len2 = distance_squared(pChr->GetPos(), GetPos());

		if(Len2 < HitRange2)
			Hit(pChr);

		// selfdestruction
		if(Len2 < ClosestLength)
		{
			ClosestLength = Len2;
			pKiller = pChr;
		}
	}

	if(pKiller)
	{
		Die(pKiller);
	}

	// reloading in progress
	if(m_ReloadCounter > 0)
	{
		m_ReloadCounter--;

		if(m_Radius > 15.0f) // shrink radius
		{
			m_Radius -= m_RadiusGrowthRate;
			if(m_Radius < 15.0f)
			{
				m_Radius = 15.0f;
			}
		}
		return; // some reload tick-cycles necessary
	}

	// Reloading finished, warm up in progress
	if(m_WarmUpCounter > 0)
	{
		m_WarmUpCounter--;

		if(m_Radius < 45.0f)
		{
			m_Radius += m_RadiusGrowthRate;
			if(m_Radius > 45.0f)
				m_Radius = 45.0f;
		}

		return; // some warmup tick-cycles necessary
	}

	AttackTargets();
}

void CTurret::AttackTargets()
{
	// warmup finished, ready to find target
	for(TEntityPtr<CIcCharacter> pChr = GameWorld()->FindFirst<CIcCharacter>(); pChr; ++pChr)
	{
		if(!m_ammunition)
			break;

		if(!pChr->IsInfected() || !pChr->CanDie())
			continue;

		float Len = distance(pChr->m_Pos, m_Pos);

		// attack zombie
		if(Len < (float)Config()->m_InfTurretRadarRange) // 800
		{
			if(GameServer()->Collision()->IntersectLineWeapon(m_Pos, pChr->m_Pos, nullptr, nullptr))
			{
				continue;
			}

			vec2 Direction = normalize(pChr->m_Pos - m_Pos);
			m_foundTarget = true;

			switch(m_Type)
			{
			case LASER:
			{
				CIcLaser *pLaser = CIcLaser::MakeLaser(GameServer(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, GetOwner(), GetDamage(), EInfclassWeapon::LASER_TURRET);
				pLaser->SetSnapType(m_SnapLaserType);
				m_ammunition--;
				break;
			}
			case PLASMA:
			{
				CPlasma *pPlasma = new CPlasma(GameServer(), m_Pos, GetOwner(), pChr->GetCid(), Direction, 0, 1);
				pPlasma->SetDamageType(EDamageType::TURRET_PLASMA);
				m_ammunition--;
				break;
			}
			}

			GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE);
		}
	}

	// either the turret found one target (single projectile) or it is out of ammo due to fire at different targets (multi projectile)
	if(!m_ammunition || m_foundTarget)
	{
		// Reload ammo
		Reload();

		m_WarmUpCounter = Server()->TickSpeed() * Config()->m_InfTurretWarmUpDuration;
		m_ammunition = Config()->m_InfTurretAmmunition;
		m_foundTarget = false;
	}
}

void CTurret::Reload()
{
	m_ReloadCounter = Server()->TickSpeed() * m_ReloadDuration;
}

void CTurret::Snap(int SnappingClient)
{
	if(!DoSnapForClient(SnappingClient))
		return;

	if(Server()->GetClientInfclassVersion(SnappingClient))
	{
		CNetObj_InfClassObject *pInfClassObject = SnapInfClassObject();
		if(!pInfClassObject)
			return;

		pInfClassObject->m_StartTick = m_StartTick;
	}

	const CIcPlayer *pPlayer = GameController()->GetPlayer(SnappingClient);
	const bool AntiPing = pPlayer && pPlayer->GetAntiPingEnabled();
	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	CSnapContext Context(SnappingClientVersion);

	float time = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();
	float angle = fmodf(time * pi / 2, 2.0f * pi);
	GameServer()->SnapLaserObject(Context, GetId(), m_Pos, m_Pos, Server()->Tick(), GetOwner(), m_SnapLaserType);

	int Dots = AntiPing ? 2 : std::size(m_Ids);
	for(int i = 0; i < Dots; i++)
	{
		float shiftedAngle = angle + 2.0 * pi * i / static_cast<float>(Dots);
		vec2 Direction = vec2(cos(shiftedAngle), sin(shiftedAngle));
		GameController()->SendHammerDot(m_Pos + Direction * m_Radius, m_Ids[i]);
	}
}

void CTurret::Hit(CIcCharacter *pCharacter)
{
	pCharacter->TakeDamage(vec2(0.f, 0.f), Config()->m_InfTurretSelfDestructDmg, GetOwner(), EDamageType::TURRET_DESTRUCTION);
	GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE);
}

void CTurret::Die(CIcCharacter *pKiller)
{
	if(!IsDestructable())
		return;

	int ClientId = pKiller->GetCid();
	GameServer()->SendChatTarget_Localization(ClientId, CHATCATEGORY_SCORE, _("You destroyed {str:PlayerName}'s turret!"),
		"PlayerName", Server()->ClientName(GetOwner()),
		nullptr);
	GameServer()->SendChatTarget_Localization(GetOwner(), CHATCATEGORY_SCORE, _("{str:PlayerName} has destroyed your turret!"),
		"PlayerName", Server()->ClientName(ClientId),
		nullptr);

	// increase score
	Server()->RoundStatistics()->OnScoreEvent(ClientId, EScoreEvent::DESTROY_TURRET, pKiller->GetPlayerClass(), Server()->ClientName(ClientId), GameServer()->Console());
	GameServer()->SendScoreSound(pKiller->GetCid());
	Reset();
}

void CTurret::SetReloadDuration(float Seconds)
{
	m_ReloadDuration = Seconds;
}

int CTurret::GetDamage() const
{
	return m_Damage.value_or(g_Config.m_InfTurretDmgHealthLaser);
}

void CTurret::SetDamage(int Damage)
{
	m_Damage = Damage;
	m_SnapLaserType = Damage > g_Config.m_InfTurretDmgHealthLaser ? LASERTYPE_FREEZE : -1;
}
