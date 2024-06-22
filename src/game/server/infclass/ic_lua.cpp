#include "ic_gamecontroller.h"

#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/entities/ic_door.h>
#include <game/server/infclass/entities/ic_entity.h>
#include <game/server/infclass/ic_player.h>

#include <engine/lua.h>

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

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

void CIcGameController::RegisterLuaBindings()
{
	lua_State *L = Lua()->GetLuaState();

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
			.addFunction("AddDoor", &CIcGameController::AddDoor)
		.endClass()
		.beginNamespace("Game")
			.addProperty("Controller", &pGameController, false)
		.endNamespace();
}
