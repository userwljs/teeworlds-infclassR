#include "medic-laser.h"

#include <engine/server/roundstatistics.h>
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/infclass/damage_type.h>
#include <game/infclass/weapons.h>
#include <game/server/gamecontext.h>

#include <game/server/infclass/infcgamecontroller.h>
#include <game/server/infclass/infcplayer.h>

#include "ic_character.h"

void CMedicLaser::OnFired(CIcCharacter *pCharacter, WeaponFireContext *pFireContext, float StartEnergy)
{
	if(pFireContext->AmmoAvailable < 10)
	{
		pFireContext->FireAccepted = false;
		pFireContext->NoAmmo = true;
		return;
	}

	CMedicLaser *pLaser = new CMedicLaser(pCharacter->GameContext(), pCharacter->GetPos(), pCharacter->GetDirection(), StartEnergy, pCharacter->GetCid(), pFireContext->InfClassWeapon);
	pLaser->DoBounce();
	pFireContext->AmmoConsumed = 5;

	pCharacter->GameServer()->CreateSound(pCharacter->GetPos(), SOUND_LASER_FIRE);
}

CMedicLaser::CMedicLaser(CGameContext *pGameContext, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, EInfclassWeapon Weapon)
	: CInfClassLaser(pGameContext, Pos, Direction, StartEnergy, Owner, 0, Weapon)
{
}

bool CMedicLaser::OnCharacterHit(CIcCharacter *pHit)
{
	CIcCharacter *pInfected = pHit;
	CIcCharacter *pMedic = GetOwnerCharacter();
	if(!pMedic)
		return false;

	pMedic->TakeAmmo(WEAPON_LASER, 5);
	bool Tranquilizer = m_Weapon == EInfclassWeapon::TRANQUILIZER_RIFLE;
	if(Tranquilizer)
	{
		float Dose = Config()->m_InfTranquilizerDose;
		float WeightRate = (10 + pHit->GetMaxArmor()) / 20.0f;
		float EffectDuration = Dose / std::sqrt(WeightRate);
		if(EffectDuration < 1.0f)
		{
			// Survival dose is 7.5 which means 7.5 ^ 2 * 20 = 1125 HP max
			GameServer()->SendBroadcast_Localization(GetOwner(), EBroadcastPriority::GAMEANNOUNCE,
				BROADCAST_DURATION_GAMEANNOUNCE,
				_("The target is too strong, the tranquilizer won't work"),
				nullptr);
			return true;
		}
		pHit->PutToSleep(EffectDuration, GetOwner());
		return true;
	}

	int MinimumHP = Config()->m_InfRevivalDamage + 1;
	int MinimumInfected = GameController()->MinimumInfectedForRevival();

	if(pMedic->GetHealthArmorSum() < MinimumHP)
	{
		GameServer()->SendBroadcast_Localization(GetOwner(), EBroadcastPriority::GAMEANNOUNCE,
			BROADCAST_DURATION_GAMEANNOUNCE,
			_("You need at least {int:MinimumHP} HP"),
			"MinimumHP", &MinimumHP,
			nullptr
		);
	}
	else if(GameController()->GetInfectedCount() < MinimumInfected)
	{
		GameServer()->SendBroadcast_Localization(GetOwner(), EBroadcastPriority::GAMEANNOUNCE,
			BROADCAST_DURATION_GAMEANNOUNCE,
			_("Too few infected (less than {int:MinimumInfected})"),
			"MinimumInfected", &MinimumInfected,
			nullptr
		);
	}
	else if(pHit->GetArmor() > 10)
	{
		GameServer()->SendBroadcast_Localization(GetOwner(), EBroadcastPriority::GAMEANNOUNCE,
			BROADCAST_DURATION_GAMEANNOUNCE,
			_("The target is too strong, the cure won't work"),
			nullptr);
	}
	else
	{
		EPlayerClass PreviousClass = pHit->GetPlayer()->GetPreviousHumanClass();
		if(PreviousClass == EPlayerClass::Invalid)
		{
			PreviousClass = EPlayerClass::Medic;
		}

		GameController()->MaybeDropPickup(pInfected);
		pInfected->GetPlayer()->SetClass(PreviousClass);
		pInfected->Unfreeze();
		pInfected->ResetBlindness();
		pInfected->CancelSlowMotion();
		pInfected->SetHealthArmor(1, 0);
		const float ReviverHelperDuration = 45;
		pInfected->AddHelper(pMedic->GetCid(), ReviverHelperDuration);
		pMedic->TakeDamage(vec2(0.f, 0.f), Config()->m_InfRevivalDamage * 2, GetOwner(), EDamageType::MEDIC_REVIVAL);

		GameServer()->SendChatTarget_Localization(-1, CHATCATEGORY_HUMANS,
			_("Medic {str:MedicName} revived {str:RevivedName}"),
			"MedicName", Server()->ClientName(pMedic->GetCid()),
			"RevivedName", Server()->ClientName(pInfected->GetCid()),
			nullptr
		);
		int ClientId = pMedic->GetCid();
		Server()->RoundStatistics()->OnScoreEvent(ClientId, SCOREEVENT_MEDIC_REVIVE, pMedic->GetPlayerClass(), Server()->ClientName(ClientId), GameServer()->Console());
	}
	return true;
}
