#include "ic_gamecontroller.h"

#if CONF_LUA
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/entities/ic_door.h>
#include <game/server/infclass/entities/ic_entity.h>
#include <game/server/infclass/ic_player.h>

#include <engine/lua.h>

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

struct CLuaPlayersNumber
{
	int Humans{};
	int Infected{};
	int Spectators{};
};

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
			// .addFunction("SetClass", &CBotPlayer::SetClass)
			.addFunction("SetMaxHP", &CIcPlayer::SetMaxHP)
			.addFunction("IsInfected", &CIcPlayer::IsInfected)
		.endClass()
		.deriveClass<CIcCharacter, CCharacter>("CIcCharacter")
			.addFunction("IsInfected", &CIcCharacter::IsInfected)
			.addFunction("SetInvincible", &CIcCharacter::SetInvincible)
		.endClass();

	static CIcGameController *pGameController = nullptr;
	pGameController = this;

	luabridge::getGlobalNamespace(L)
		.beginClass<CIcEntity>("CInfCEntity")
			.addFunction("Destroy", &CIcEntity::Destroy)
			.addFunction("GetPosition", &CIcEntity::GetPos)
		.endClass()
		.deriveClass<CDoor, CIcEntity>("CDoor")
			.addFunction("SetOpen", &CDoor::SetOpen)
		.endClass()
		.beginClass<CIcGameController>("CIcGameController")
			.addFunction("GetPlayer", &CIcGameController::GetPlayer)
			.addFunction("GetCharacter", &CIcGameController::GetCharacter)
			.addFunction("GetSecondsElapsed", &CIcGameController::GetSecondsElapsed)
			.addFunction("GetSecondsRemaining", &CIcGameController::GetSecondsRemaining)
			.addFunction("GetPlayersNumber", &CIcGameController::GetPlayersNumber_Lua)
			.addFunction("GetHumanSpawns", &CIcGameController::GetHumanSpawns)
			.addFunction("GetInfectedSpawns", &CIcGameController::GetInfectedSpawns)
			.addFunction("SetHumanSpawnEnabled", &CIcGameController::SetHumanSpawnEnabled)
			.addFunction("SetInfectedSpawnEnabled", &CIcGameController::SetInfectedSpawnEnabled)
			.addFunction("AddDoor", &CIcGameController::AddDoor)
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
