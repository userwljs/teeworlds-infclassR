#include "ic_gamecontroller.h"

#if CONF_LUA
#include <game/server/infclass/bot-player.h>
#include <game/server/infclass/entities/control-point.h>
#include <game/server/infclass/entities/engineer-wall.h>
#include <game/server/infclass/entities/hero-flag.h>
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/entities/ic_door.h>
#include <game/server/infclass/entities/ic_entity.h>
#include <game/server/infclass/entities/looper-wall.h>
#include <game/server/infclass/entities/scientist-mine.h>
#include <game/server/infclass/entities/turret.h>
#include <game/server/infclass/ic_player.h>
#include <game/server/infclass/survival.h>
#include <game/server/map_info.h>

#include <engine/lua.h>
#include <engine/server/lua_callback.h>
#include <engine/server/roundstatistics.h>

#include <base/tl/ic_enum.h>

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

struct CLuaPlayersNumber
{
	int Humans{};
	int Infected{};
	int Spectators{};
};

namespace
{

void SetPlayerClass_Lua(CIcPlayer *pPlayer, const char *pClass)
{
	if (!pClass)
		return;

	EPlayerClass Class = fromString<EPlayerClass>(pClass);
	if (Class == EPlayerClass::Invalid)
		return;

	pPlayer->SetClass(Class);
}

const char *GetPlayerClass_Lua(const CIcPlayer *pPlayer)
{
	if (!pPlayer)
		return nullptr;

	return toString(pPlayer->GetClass());
}

int GetPlayerScore_Lua(const CIcPlayer *pPlayer)
{
	return pPlayer->GetScore(SERVER_DEMO_CLIENT);
}

void SetPlayerScore_Lua(CIcPlayer *pPlayer, int Score)
{
	return pPlayer->Server()->RoundStatistics()->SetPlayerScore(pPlayer->GetCid(), Score);
}

vec2 EntityGetPosition_Lua(const CIcEntity *pEntity)
{
	return pEntity->GetPos();
}

void EntitySetPosition_Lua(CIcEntity *pEntity, vec2 Position)
{
	pEntity->SetPos(Position);
}

const char *GetRoundType_Lua(const CIcGameController *pController)
{
	return toString(pController->GetRoundType());
}

CMapInfoEx *GetMapInfo_Lua(const CIcGameController *pController, const char *pMapName)
{
	return pController->GetMapInfo(pMapName);
}

float GetSecondsAfterInfection_Lua(const CIcGameController *pController)
{
	return pController->GetInfectionTick() / static_cast<float>(SERVER_TICK_SPEED);
}

void FinishRound_Lua(CIcGameController *pController)
{
	pController->EndRound(ERoundEndReason::FINISHED);
}

void CancelRound_Lua(CIcGameController *pController)
{
	pController->EndRound(ERoundEndReason::CANCELED);
}

bool GetPlayerClassEnabled_Lua(const CIcGameController *pController, const char *pClassName)
{
	EPlayerClass PlayerClass = fromString<EPlayerClass>(pClassName);
	if(PlayerClass == EPlayerClass::Invalid)
		return false;

	return pController->GetPlayerClassEnabled(PlayerClass);
}

bool SetPlayerClassEnabled_Lua(CIcGameController *pController, const char *pClassName, bool Enabled)
{
	EPlayerClass PlayerClass = fromString<EPlayerClass>(pClassName);
	if(PlayerClass == EPlayerClass::Invalid)
		return false;

	return pController->SetPlayerClassEnabled(PlayerClass, Enabled);
}

bool ResetPlayerClassEnabled_Lua(CIcGameController *pController, const char *pClassName)
{
	EPlayerClass PlayerClass = fromString<EPlayerClass>(pClassName);
	if(PlayerClass == EPlayerClass::Invalid)
		return false;

	return pController->ResetPlayerClassEnabled(PlayerClass);
}

CScientistMine *AddSciMine(const CIcGameController *pController)
{
	return new CScientistMine(pController->GameServer(), {}, -1);
}

CPlacedObject *AddLaserWall(const CIcGameController *pController)
{
	return new CEngineerWall(pController->GameServer(), {}, -1);
}

CPlacedObject *AddLooperWall(const CIcGameController *pController)
{
	return new CLooperWall(pController->GameServer(), {}, -1);
}

CTurret *AddTurret(const CIcGameController *pController)
{
	return new CTurret(pController->GameServer(), {}, -1, CTurret::LASER);
}

void TweaksArray_Add_Lua(TweaksArray *pArray, const char *pTweakName)
{
	pArray->Add(fromString<EBotTweak>(pTweakName));
}

void TweaksArray_Remove_Lua(TweaksArray *pArray, const char *pTweakName)
{
	pArray->RemoveOne(fromString<EBotTweak>(pTweakName));
}

bool TweaksArray_Contains_Lua(const TweaksArray *pArray, const char *pTweakName)
{
	return pArray->Contains(fromString<EBotTweak>(pTweakName));
}

const char *TweaksArray_At_Lua(const TweaksArray *pArray, int Index)
{
	return toString(pArray->At(Index));
}

void SetSurvivalBotConfigurationClass_Lua(SurvivalBotConfiguration *pConfiguration, const char *pClass)
{
	EPlayerClass Class = fromString<EPlayerClass>(pClass);
	if (Class == EPlayerClass::Invalid)
		return;

	pConfiguration->Class = Class;
}

const char *GetSurvivalBotConfigurationClass_Lua(const SurvivalBotConfiguration *pConfiguration)
{
	if (!pConfiguration)
		return nullptr;

	return toString(pConfiguration->Class);
}

void SetSurvivalBotConfigurationTag_Lua(SurvivalBotConfiguration *pConfiguration, const char *pTag)
{
	str_copy(pConfiguration->Tag, pTag);
}

const char *GetSurvivalBotConfigurationTag_Lua(const SurvivalBotConfiguration *pConfiguration)
{
	return pConfiguration->Tag;
}

void SetSurvivalBotConfigurationSpawnSecond_Lua(SurvivalBotConfiguration *pConfiguration, float Second)
{
	pConfiguration->SpawnMinTick = Second * SERVER_TICK_SPEED;
}

float GetSurvivalBotConfigurationSpawnSecond_Lua(const SurvivalBotConfiguration *pConfiguration)
{
	return pConfiguration->SpawnMinTick / static_cast<float>(SERVER_TICK_SPEED);
}

TweaksArray *SurvivalBotConfigurationClass_GetTweaks_Lua(SurvivalBotConfiguration *pConfiguration)
{
	if (!pConfiguration)
		return nullptr;

	return &pConfiguration->Tweaks;
}

void PrepareSurvival_Lua(CIcGameController *pController)
{
	pController->PrepareSurvival(0);
}

} // namespace

void CIcGameController::ConExecLua(IConsole::IResult *pResult, void *pUserData)
{
	CIcGameController *pSelf = (CIcGameController *)pUserData;
	const char *pFileName = pResult->GetString(0);
	pSelf->GameServer()->Lua()->ExecScriptFile(pFileName);
}

void CIcGameController::ConLua(IConsole::IResult *pResult, void *pUserData)
{
	CIcGameController *pSelf = (CIcGameController *)pUserData;
	const char *pCode = pResult->GetString(0);
	pSelf->GameServer()->Lua()->ExecScript(pCode);
}

const array<vec2> *CIcGameController::GetHeroFlagPositions() const
{
	return &m_HeroFlagPositions;
}

const std::vector<vec2> *CIcGameController::GetHumanSpawns() const
{
	int Type = 1;
	return &m_avSpawnPoints[Type];
}

const std::vector<vec2> *CIcGameController::GetInfectedSpawns() const
{
	int Type = 0;
	return &m_avSpawnPoints[Type];
}

void CIcGameController::SetSpawnEnabled(int Index, bool Enabled, int Type)
{
	std::vector<int> &EnabledPointsIndices = m_EnabledSpawnPoints[Type];
	if(Index < 1)
	{
		return;
	}

	std::size_t FixedSpawnIndex = Index - 1;
	if(FixedSpawnIndex >= m_avSpawnPoints[Type].size())
	{
		return;
	}

	auto it = std::find(EnabledPointsIndices.begin(), EnabledPointsIndices.end(), FixedSpawnIndex);
	if(Enabled)
	{
		if(it == EnabledPointsIndices.end())
			EnabledPointsIndices.push_back(FixedSpawnIndex);
	}
	else
	{
		if(it != EnabledPointsIndices.end())
			EnabledPointsIndices.erase(it);
	}
}

void CIcGameController::SetHumanSpawnEnabled(int Index, bool Enabled)
{
	int HumansIndex = 1;
	SetSpawnEnabled(Index, Enabled, HumansIndex);
}

void CIcGameController::SetInfectedSpawnEnabled(int Index, bool Enabled)
{
	int InfectedIndex = 0;
	SetSpawnEnabled(Index, Enabled, InfectedIndex);
}

CLuaPlayersNumber CIcGameController::GetPlayersNumber_Lua(bool IncludeBots)
{
	CLuaPlayersNumber Result;

	CIcPlayerIterator<PLAYERITER_COND_READY> Iter(GameServer()->m_apPlayers);
	while(Iter.Next())
	{
		if(!IncludeBots && Iter.Player()->IsBot())
			continue;

		if(Iter.Player()->IsSpectator())
			Result.Spectators++;
		else if(Iter.Player()->IsInfected())
			Result.Infected++;
		else
			Result.Humans++;
	}

	return Result;
}

void CIcGameController::UpdateHeroFlags_Lua()
{
	if(!Lua()->HasGlobalCallable("Is_flag_position_valid"))
		return;

	for(TEntityPtr<CHeroFlag> pFlag = GameWorld()->FindFirst<CHeroFlag>(); pFlag; ++pFlag)
	{
		if(pFlag->IsAvailable())
		{
			CIcEntity *pFlagEntity = pFlag;
			std::optional<bool> allowed = RunCallbackWithResult<bool>(Lua()->GetLuaState(), "Is_flag_position_valid", pFlagEntity);
			if(allowed.has_value() && allowed.value() != true)
			{
				pFlag->FindPosition();
			}
		}
	}
}

void CIcGameController::RegisterLuaBindings()
{
	lua_State *L = Lua()->GetLuaState();

	luabridge::getGlobalNamespace(L)
		.beginClass< CLuaPlayersNumber >("PlayersNumber")
			.addConstructor <void (*) ()> ()
			.addProperty("Humans", &CLuaPlayersNumber::Humans)
			.addProperty("Infected", &CLuaPlayersNumber::Infected)
			.addProperty("Spectators", &CLuaPlayersNumber::Spectators)
		.endClass();

	luabridge::getGlobalNamespace(L)
		.deriveClass<CIcPlayer, CPlayer>("CIcPlayer")
			.addProperty("Class", &GetPlayerClass_Lua, &SetPlayerClass_Lua)
			.addProperty("MaxHP", &CIcPlayer::GetMaxHP, &CIcPlayer::SetMaxHP)
			.addProperty("Tag", &CIcPlayer::GetTag)
			.addFunction("ApplyMaxHP", &CIcPlayer::ApplyMaxHP)
			.addFunction("IsInfected", &CIcPlayer::IsInfected)
			.addProperty("Score", &GetPlayerScore_Lua, &SetPlayerScore_Lua)
			.addProperty("Deaths", &CIcPlayer::GetDeaths)

			.addFunction("HookProtectionEnabled", &CIcPlayer::HookProtectionEnabled)
			.addFunction("SetHookProtection", &CIcPlayer::SetHookProtection)

			.addFunction("SpecialCameraIsActive", &CIcPlayer::SpecialCameraIsActive)
			.addFunction("ResetSpecialCamera", &CIcPlayer::ResetSpecialCamera)
			.addFunction("GetSpecialCameraTargetCid", &CIcPlayer::GetSpecialCameraTargetCid)
			.addFunction("SetSpecialCameraTargetCid", &CIcPlayer::SetSpecialCameraTargetCid)
		.endClass()
		.deriveClass<CIcCharacter, CCharacter>("CIcCharacter")
			.addFunction("IsInfected", &CIcCharacter::IsInfected)
			.addFunction("Heal", &CIcCharacter::Heal)

			.addFunction("GiveArmor", &CIcCharacter::GiveArmor)
			.addFunction("GiveHealth", &CIcCharacter::GiveHealth)

			.addFunction("GetDropLevel", &CIcCharacter::GetDropLevel)
			.addFunction("SetDropLevel", &CIcCharacter::SetDropLevel)

			.addFunction("IsPassenger", &CIcCharacter::IsPassenger)
			.addFunction("HasPassenger", &CIcCharacter::HasPassenger)
			.addFunction("GetPassenger", &CIcCharacter::GetPassenger)
			.addFunction("GetTaxi", &CIcCharacter::GetTaxi)
			.addFunction("GetTaxiDriver", &CIcCharacter::GetTaxiDriver)

			.addFunction("IsInvisible", &CIcCharacter::IsInvisible)
			.addFunction("MakeVisible", &CIcCharacter::MakeVisible)
			.addFunction("GrantInvisibility", &CIcCharacter::GrantInvisibility)
			.addFunction("SetInvincible", &CIcCharacter::SetInvincible)
			.addFunction("IsSleeping", &CIcCharacter::IsSleeping)
			.addFunction("PutToSleep", &CIcCharacter::PutToSleep)
			.addFunction("CancelSleep", &CIcCharacter::CancelSleep)
			.addFunction("AwakenedBy", &CIcCharacter::AwakenedBy)
			.addFunction("MakeBlind", &CIcCharacter::MakeBlind)
			.addFunction("ResetBlindness", &CIcCharacter::ResetBlindness)
			.addFunction("IsFrozen", &CIcCharacter::IsFrozen)
			.addProperty("FreezeStartTick", &CIcCharacter::FreezeStartTick)
			.addFunction("Unfreeze", &CIcCharacter::Unfreeze)
			.addFunction("TryUnfreeze", &CIcCharacter::TryUnfreeze)
			.addFunction("IsInSlowMotion", &CIcCharacter::IsInSlowMotion)
			.addFunction("MakeSlow", &CIcCharacter::SlowMotionEffect)
			.addFunction("IsPoisoned", &CIcCharacter::IsPoisoned)
			.addFunction("ResetPoisonEffect", &CIcCharacter::ResetPoisonEffect)
			.addFunction("LoveEffect", &CIcCharacter::LoveEffect)
			.addFunction("IsInLove", &CIcCharacter::IsInLove)
			.addFunction("ResetLoveEffect", &CIcCharacter::CancelLoveEffect)
			.addFunction("SetAntiFireDuration", &CIcCharacter::SetAntiFireDuration)
		.endClass();

	luabridge::getGlobalNamespace(L)
		.beginClass<CMapInfoEx>("MapInfo")
			.addProperty("MinimumPlayers", &CMapInfoEx::MinimumPlayers)
			.addProperty("MaximumPlayers", &CMapInfoEx::MaximumPlayers)
			.addProperty("Enabled", &CMapInfoEx::mEnabled)
			.addProperty("Name", &CMapInfoEx::Name)
		.endClass();

	luabridge::getGlobalNamespace(L)
		.deriveClass<CBaseBotPlayer, CIcPlayer>("CBaseBotPlayer")
			.addProperty("Lives", &CBaseBotPlayer::Lives, &CBaseBotPlayer::SetLives)
			.addProperty("MaxLives", &CBaseBotPlayer::MaxLives, &CBaseBotPlayer::SetMaxLives)
			.addProperty("RespawnInterval", &CBaseBotPlayer::GetRespawnInterval, &CBaseBotPlayer::SetRespawnInterval)
		.endClass();

	luabridge::getGlobalNamespace(L).deriveClass<CBotPlayer, CBaseBotPlayer>("CBotPlayer");

	static CIcGameController *pGameController = nullptr;
	pGameController = this;

	luabridge::getGlobalNamespace(L)
		.beginClass<TweaksArray>("TweaksArray")
			.addFunction("Add", &TweaksArray_Add_Lua)
			.addFunction("Remove", &TweaksArray_Remove_Lua)
			.addFunction("Contains", &TweaksArray_Contains_Lua)
			.addFunction("At", &TweaksArray_At_Lua)
			.addFunction("Size", &TweaksArray::Size)
		.endClass()
		.beginClass<SurvivalGameConfiguration>("SurvivalConfiguration")
			.addProperty("MaxPlayers", &SurvivalGameConfiguration::MaxPlayers)
			.addProperty("Hardmode", &SurvivalGameConfiguration::HardMode)
			.addFunction("Reset", &SurvivalGameConfiguration::Reset)
		.endClass()
		.beginClass<SurvivalBotConfiguration>("SurvivalBotConfiguration")
			.addProperty("Class", &GetSurvivalBotConfigurationClass_Lua, &SetSurvivalBotConfigurationClass_Lua)
			.addProperty("Tag", &GetSurvivalBotConfigurationTag_Lua, &SetSurvivalBotConfigurationTag_Lua)
			.addProperty("SpawnSecond", &GetSurvivalBotConfigurationSpawnSecond_Lua, &SetSurvivalBotConfigurationSpawnSecond_Lua)
			.addProperty("Lives", &SurvivalBotConfiguration::Lives)
			.addProperty("HP", &SurvivalBotConfiguration::HP)
			.addProperty("DropLevel", &SurvivalBotConfiguration::DropLevel)
			.addProperty("RespawnInterval", &SurvivalBotConfiguration::RespawnInterval)
			.addFunction("GetTweaks", &SurvivalBotConfigurationClass_GetTweaks_Lua)
		.endClass()
		.beginClass<CIcEntity>("CIcEntity")
			.addFunction("Destroy", &CIcEntity::MarkForDestroy)
			.addFunction("MarkForDestroy", &CIcEntity::MarkForDestroy)
			.addProperty("Position", &EntityGetPosition_Lua, &EntitySetPosition_Lua)
			.addProperty("Velocity", &CIcEntity::GetVelocity, &CIcEntity::SetVelocity)
			.addProperty("Lifespan", &CIcEntity::GetLifespan, &CIcEntity::SetLifespan)
			.addProperty("ProximityRadius", &CIcEntity::m_ProximityRadius)
			.addFunction("MoveTo", &CIcEntity::MoveTo)
		.endClass()
		.deriveClass<CPlacedObject, CIcEntity>("CPlacedObject")
			.addFunction("HasSecondPosition", &CPlacedObject::HasSecondPosition)
			.addProperty("SecondPosition", &CPlacedObject::SecondPosition, &CPlacedObject::SetSecondPosition)
			.addProperty("MaxLength", &CPlacedObject::MaxLength, &CPlacedObject::SetMaxLength)
		.endClass()
		.deriveClass<CControlPoint, CPlacedObject>("CControlPoint")
			.addFunction("IsTaken", &CControlPoint::IsTaken)
			.addFunction("IsInfected", &CControlPoint::IsInfected)
			.addFunction("SetNextEffectTime", &CControlPoint::SetNextEffectTime)
		.endClass()
		.deriveClass<CDoor, CPlacedObject>("CDoor")
			.addFunction("SetOpen", &CDoor::SetOpen)
		.endClass()
		.deriveClass<CScientistMine, CPlacedObject>("CScientistMine")
			.addFunction("SetExplosionRadius", &CScientistMine::SetExplosionRadius)
		.endClass()
		.deriveClass<CTurret, CPlacedObject>("CTurret")
			.addProperty("ReloadDuration", &CTurret::GetReloadDuration, &CTurret::SetReloadDuration)
			.addProperty("Damage", &CTurret::GetDamage, &CTurret::SetDamage)
			.addProperty("Destructable", &CTurret::IsDestructable, &CTurret::SetDestructable)
		.endClass()
		.deriveClass<CIcGameController, IGameController>("CIcGameController")
			.addProperty("GameType", &CIcGameController::GameType, &CIcGameController::SetGameType)
			.addProperty("RoundType", &GetRoundType_Lua)
			.addProperty("RoundStartTick", &CIcGameController::GetRoundStartTick)
			.addProperty("InfectionTick", &CIcGameController::GetInfectionTick)
			.addProperty("InfectionStartTick", &CIcGameController::GetInfectionStartTick)
			.addProperty("RoundTick", &CIcGameController::GetRoundTick)
			.addProperty("TimeLimitSeconds",
				&CIcGameController::GetTimeLimitSeconds,
				&CIcGameController::SetTimeLimitSeconds)
			.addProperty("InfectionDelaySeconds",
				&CIcGameController::GetInfectionDelay,
				&CIcGameController::SetInfectionDelay)
			.addProperty("WinCheckEnabled",
				&CIcGameController::IsWinCheckEnabled,
				&CIcGameController::SetWinCheckEnabled)
			.addProperty("VotesEnabled",
				&CIcGameController::GetVotesEnabled,
				&CIcGameController::SetVotesEnabled)
			.addProperty("RoundMinimumPlayers",
				&CIcGameController::GetMinPlayers,
				&CIcGameController::SetRoundMinimumPlayers)
			.addProperty("RoundMinimumInfected",
				&CIcGameController::GetMinimumInfected,
				&CIcGameController::SetRoundMinimumInfected)
			.addFunction("AddMapInfo", &CIcGameController::AddMapInfo)
			.addFunction("GetMapInfo", &GetMapInfo_Lua)
			.addFunction("GetPlayer", &CIcGameController::GetPlayer)
			.addFunction("GetCharacter", &CIcGameController::GetCharacter)
			.addFunction("GetSecondsAfterInfection", &GetSecondsAfterInfection_Lua)
			.addFunction("GetSecondsElapsed", &CIcGameController::GetSecondsElapsed)
			.addFunction("GetSecondsRemaining", &CIcGameController::GetSecondsRemaining)
			.addFunction("GetPlayersNumber", &CIcGameController::GetPlayersNumber_Lua)
			.addFunction("GetHeroFlagPositions", &CIcGameController::GetHeroFlagPositions)
			.addFunction("GetHumanSpawns", &CIcGameController::GetHumanSpawns)
			.addFunction("GetInfectedSpawns", &CIcGameController::GetInfectedSpawns)
			.addFunction("SetHumanSpawnEnabled", &CIcGameController::SetHumanSpawnEnabled)
			.addFunction("SetInfectedSpawnEnabled", &CIcGameController::SetInfectedSpawnEnabled)
			.addFunction("IsPositionAvailableForHumans", &CIcGameController::IsPositionAvailableForHumans)
			.addFunction("UpdateHeroFlags", &CIcGameController::UpdateHeroFlags_Lua)

			.addFunction("StartRound", &CIcGameController::StartRound)
			.addFunction("FinishRound", &FinishRound_Lua)
			.addFunction("CancelRound", &CancelRound_Lua)
			.addFunction("QueueRound", &CIcGameController::ConQueueRound)
			.addFunction("DoWarmup", &CIcGameController::DoWarmup)
			.addFunction("PrepareSurvival", &PrepareSurvival_Lua)

			.addFunction("GetPlayerClassEnabled", &GetPlayerClassEnabled_Lua)
			.addFunction("SetPlayerClassEnabled", &SetPlayerClassEnabled_Lua)
			.addFunction("ResetPlayerClassEnabled", &ResetPlayerClassEnabled_Lua)
			.addFunction("ResetPlayerClassesEnablement", &CIcGameController::ResetPlayerClassesEnablement)

			.addFunction("AddControlPoint", &CIcGameController::AddControlPoint)
			.addFunction("AddDoor", &CIcGameController::AddDoor)
			.addFunction("AddSciMine", &AddSciMine)
			.addFunction("AddLaserWall", &AddLaserWall)
			.addFunction("AddLooperWall", &AddLooperWall)
			.addFunction("AddTurret", &AddTurret)

			.addFunction("AddBot", &CIcGameController::AddBot_Lua)
			.addFunction("GetBot", &CIcGameController::GetBot)
			.addFunction("RemoveBot", &CIcGameController::RemoveBot_Lua)
			.addFunction("RemoveAllBots", &CIcGameController::RemoveBots)

			.addFunction("SurvivalGetGameConfiguration", &CIcGameController::SurvivalGetMutableGameConfiguration)
			.addFunction("SurvivalAddWave", &CIcGameController::SurvivalAddWave)
			.addFunction("SurvivalAddBot", &CIcGameController::SurvivalAddBot)
		.endClass()
		.beginNamespace("Game")
			.addProperty("Controller", &pGameController, false)
		.endNamespace();
}
#else // USE_LUA
void CIcGameController::RegisterLuaBindings()
{
}
#endif // USE_LUA
