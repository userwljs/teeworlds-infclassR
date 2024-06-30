#if CONF_LUA

#include "lua.h"

#include <base/system.h>
#include <base/vmath.h>

#include <engine/server.h>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>

#include <game/server/entities/character.h>

#include "luabinding.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

using namespace luabridge;

template<>
struct luabridge::Stack<EWeapon> : luabridge::Enum<EWeapon,
	EWeapon::WEAPON_HAMMER,
	EWeapon::WEAPON_GUN,
	EWeapon::WEAPON_SHOTGUN,
	EWeapon::WEAPON_GRENADE,
	EWeapon::WEAPON_LASER,
	EWeapon::WEAPON_NINJA>
{
};

template <typename ValueType>
ValueType ArrayAt_Lua(const array<ValueType> *pArray, int Index)
{
	int FixedIndex = Index - 1;
	if(FixedIndex < 0 || FixedIndex >= pArray->size())
	{
		return {};
	}
	return (*pArray)[FixedIndex];
}

template <typename ValueType>
ValueType VectorAt_Lua(const std::vector<ValueType> *pContainer, int Index)
{
	if (Index < 1)
		return {};

	std::size_t FixedIndex = Index - 1;
	if(FixedIndex >= pContainer->size())
	{
		return {};
	}
	return (*pContainer)[FixedIndex];
}

vec2 Vec2Sub_Lua(const vec2 &vec1, const vec2 &vec2)
{
	return vec1 - vec2;
}

vec2 Vec2Mul_Lua(const vec2 &vec1, float factor)
{
	return vec1 * factor;
}

vec2 Vec2Div_Lua(const vec2 &vec1, float factor)
{
	return vec1 / factor;
}

vec2 Vec2Normalized_Lua(const vec2 &vec)
{
	return normalize(vec);
}

float Vec2Length_Lua(const vec2 &vec)
{
	return length(vec);
}

vec2 CharacterGetPosition_Lua(const CCharacter *pCharacter)
{
	return pCharacter->GetPos();
}

void CharacterSetPosition_Lua(CCharacter *pCharacter, vec2 Position)
{
	pCharacter->SetPosition(Position);
}

void GameContext_SendBroadcast_Lua(CGameContext *pGameContext, int To, const char *pText, int Priority, float LifeSpan)
{
	pGameContext->SendBroadcast(To, pText, static_cast<EBroadcastPriority>(Priority), LifeSpan * SERVER_TICK_SPEED);
}

void CLua::RegisterLuaBindings()
{
	lua_State *L = GetLuaState();

	// kill everything malicious
	static const char s_aBlacklist[][64] = {
			"os.exit",
			"os.execute",
			"os.rename",
			"os.remove",
			"os.setlocal",
			"dofile",
			"load",
			"loadfile",
			"loadstring",
	};
	for(unsigned i = 0; i < sizeof(s_aBlacklist)/sizeof(s_aBlacklist[0]); i++)
	{
		char aCmd[128];
		str_format(aCmd, sizeof(aCmd), "%s=nil", s_aBlacklist[i]);
		luaL_dostring(L, aCmd);
		if(g_Config.m_Debug)
			dbg_msg("lua", "disable: '%s'", aCmd);
	}

	luaL_dostring(L, "package.path = 'lua/?.lua'");

	// register everything
	lua_register(L, "print", CLuaBinding::LuaPrintOverride);
	lua_register(L, "Listdir", CLuaBinding::LuaListdir); // TODO: get rid of me

	luabridge::getGlobalNamespace (L)
		.beginNamespace("EWeapon")
			.addProperty("Hammer", +[] { return EWeapon::WEAPON_HAMMER; })
			.addProperty("Gun", +[] { return EWeapon::WEAPON_GUN; })
			.addProperty("Shotgun", +[] { return EWeapon::WEAPON_SHOTGUN; })
			.addProperty("Grenade", +[] { return EWeapon::WEAPON_GRENADE; })
			.addProperty("Laser", +[] { return EWeapon::WEAPON_LASER; })
			.addProperty("Ninja", +[] { return EWeapon::WEAPON_NINJA; })
		.endNamespace();

	getGlobalNamespace(L)
		// global types
		.beginClass< vector2_base<float> >("vec2")
			.addConstructor <void (*) ()> ()
			.addConstructor <void (*) (float, float)> ()
			.addFunction("__add", &vector2_base<float>::operator+)
			.addFunction("__sub", &Vec2Sub_Lua)
			.addFunction("__mul", &Vec2Mul_Lua)
			.addFunction("__div", &Vec2Div_Lua)
			.addFunction("__eq", &vector2_base<float>::operator==)
			.addProperty("x", &vector2_base<float>::x)
			.addProperty("y", &vector2_base<float>::y)

			.addFunction("Length", &Vec2Length_Lua)
			.addFunction("Normalized", &Vec2Normalized_Lua)
		.endClass()
		.beginClass< vector3_base<float> >("vec3")
			.addConstructor <void (*) ()> ()
			.addConstructor <void (*) (float, float, float)> ()
			.addFunction("__add", &vector3_base<float>::operator+)
			//.addFunction("__sub", &vector3_base<float>::operator-)
			//.addFunction("__mul", &vector3_base<float>::operator*)
			//.addFunction("__div", &vector3_base<float>::operator/)
			.addFunction("__eq", &vector3_base<float>::operator==)
			.addProperty("x", &vector3_base<float>::x)
			.addProperty("y", &vector3_base<float>::y)
			.addProperty("z", &vector3_base<float>::z)
		.endClass()
		.beginClass< vector4_base<float> >("vec4")
			.addConstructor <void (*) ()> ()
			.addConstructor <void (*) (float, float, float, float)> ()
			.addProperty("r", &vector4_base<float>::r)
			.addProperty("g", &vector4_base<float>::g)
			.addProperty("b", &vector4_base<float>::b)
			.addProperty("a", &vector4_base<float>::a)
		.endClass()

		.beginClass< vector2_base<int> >("vec2i")
			.addConstructor <void (*) (int, int)> ()
			.addFunction("__add", &vector2_base<int>::operator+)
			//.addFunction("__sub", &vector2_base<int>::operator-)
			//.addFunction("__mul", &vector2_base<int>::operator*)
			//.addFunction("__div", &vector2_base<int>::operator/)
			.addFunction("__eq", &vector2_base<int>::operator==)
			.addProperty("x", &vector2_base<int>::x)
			.addProperty("y", &vector2_base<int>::y)
		.endClass()
		.beginClass< vector3_base<int> >("vec3i")
			.addConstructor <void (*) (int, int, int)> ()
			.addFunction("__add", &vector3_base<int>::operator+)
			//.addFunction("__sub", &vector3_base<int>::operator-)
			//.addFunction("__mul", &vector3_base<int>::operator*)
			//.addFunction("__div", &vector3_base<int>::operator/)
			.addFunction("__eq", &vector3_base<int>::operator==)
			.addProperty("x", &vector3_base<int>::x)
			.addProperty("y", &vector3_base<int>::y)
			.addProperty("z", &vector3_base<int>::z)
		.endClass()
		.beginClass< vector4_base<int> >("vec4i")
			.addConstructor <void (*) (int, int, int, int)> ()
			.addProperty("r", &vector4_base<int>::r)
			.addProperty("g", &vector4_base<int>::g)
			.addProperty("b", &vector4_base<int>::b)
			.addProperty("a", &vector4_base<int>::a)
		.endClass()

		.beginClass<array<vec2>>("ArrayVec2")
			.addFunction("Size", &array<vec2>::size)
			.addFunction("At", &ArrayAt_Lua<vec2>)
		.endClass()

		.beginClass<std::vector<vec2>>("VectorVec2")
			.addFunction("Size", &std::vector<vec2>::size)
			.addFunction("At", &VectorAt_Lua<vec2>)
		.endClass()

		// Game:Players(ID).Character
		.beginClass<CCharacter>("CCharacter")
			.addProperty("CID", &CCharacter::GetCid)
			.addProperty("Position", &CharacterGetPosition_Lua, &CharacterSetPosition_Lua)
			.addFunction("GetPosition", &CCharacter::GetPos)
			.addFunction("SetPosition", &CCharacter::SetPosition)
			.addFunction("Move", &CCharacter::Move)
			.addFunction("ResetVelocity", &CCharacter::ResetVelocity)
			.addFunction("SetVelocity", &CCharacter::SetVelocity)
			.addFunction("AddVelocity", &CCharacter::AddVelocity)

			.addProperty("Health", &CCharacter::GetHealth)
			.addProperty("Armor", &CCharacter::GetArmor)
			.addProperty("MaxArmor", &CCharacter::GetMaxArmor, &CCharacter::SetMaxArmor)
			.addFunction("GetHealthArmorSum", &CCharacter::GetHealthArmorSum)
			.addFunction("SetHealthArmor", &CCharacter::SetHealthArmor)

			.addFunction("AddAmmo", &CCharacter::AddAmmo)
			.addFunction("GetAmmo", &CCharacter::GetAmmo)
			.addFunction("GiveWeapon", &CCharacter::GiveWeapon)
			.addFunction("TakeAllWeapons", &CCharacter::TakeAllWeapons)

			//.addFunction("Die", &CCharacter::Die)
			//.addFunction("TakeDamage", &CCharacter::TakeDamage)
			.addFunction("IncreaseOverallHp", &CCharacter::IncreaseOverallHp)
			//.addFunction("IncreaseHealth", &CCharacter::IncreaseHealth)
			//.addFunction("IncreaseArmor", &CCharacter::IncreaseArmor)
			//.addFunction("GiveNinja", &CCharacter::GiveNinja)
			.addFunction("SetEmote", &CCharacter::SetEmote)

			// attributes
			.addFunction("IsAlive", &CCharacter::IsAlive)
		.endClass()

		// Game:Players(ID)
		.beginClass<CPlayer>("CPlayer")
			// functions
			.addFunction("Respawn", &CPlayer::Respawn)
			.addFunction("SetTeam", &CPlayer::SetTeam)
			.addFunction("KillCharacter", &CPlayer::KillCharacter)

			// attributes
			.addProperty("Team", &CPlayer::GetTeam)
			.addProperty("CID", &CPlayer::GetCid)
			.addProperty("PlayerFlags", &CPlayer::m_PlayerFlags, false)
			.addProperty("SpectatorID", &CPlayer::m_SpectatorId, true)
			.addProperty("RespawnTick", &CPlayer::m_RespawnTick, false)
			.addProperty("DieTick", &CPlayer::m_DieTick, false)
			// TODO: add the m_TeeInfos struct (probably really easy :D) (yet better add all the structs.)
			// .addFunction("GetCharacter", &CPlayer::GetCharacter) // cleaner as variable instead of a getter function I think
		.endClass()
		.beginClass<CCollision>("CCollision")
			.addProperty("Width", &CCollision::GetWidth)
			.addProperty("Height", &CCollision::GetHeight)
			.addFunction("CheckPoint", &CCollision::CheckPointLua)
		.endClass()
		// Game.Server
		.beginClass<IServer>("IServer")
			.addProperty("Tick", &IServer::Tick)
			.addProperty("TickSpeed", &IServer::TickSpeed)

			.addFunction("GetAuthedState", &IServer::GetAuthedState)
			.addFunction("Kick", &IServer::Kick)
			.addFunction("GetClientName", &IServer::ClientName)

		.endClass()
		.beginClass<CGameContext>("CGameServer")
			.addProperty("Collision", &CGameContext::Collision)
			.addProperty("Paused", &CGameContext::IsPaused, &CGameContext::SetPaused)

			.addFunction("InsertVote", &CGameContext::InsertVote)
			.addFunction("AddVote", &CGameContext::AddVote)
			.addFunction("RemoveVote", &CGameContext::RemoveVote)
			.addFunction("ClearVotes", &CGameContext::ClearVotes)
			.addFunction("StartVote", &CGameContext::StartVote)
			.addFunction("EndVote", &CGameContext::EndVote)

			.addFunction("CreateDamageInd", &CGameContext::CreateDamageInd)
			.addFunction("CreateExplosion", &CGameContext::CreateExplosion)
			.addFunction("CreateHammerHit", &CGameContext::CreateHammerHit)
			.addFunction("CreatePlayerSpawn", &CGameContext::CreatePlayerSpawn)
			.addFunction("CreateDeath", &CGameContext::CreateDeath)
			.addFunction("CreateSound", &CGameContext::CreateSound)
			.addFunction("CreateSoundGlobal", &CGameContext::CreateSoundGlobal)

			.addFunction("SendChatTarget", &CGameContext::SendChatTarget)
			.addFunction("SendChat", &CGameContext::SendChat)
			.addFunction("SendEmoticon", &CGameContext::SendEmoticon)
			.addFunction("SendBroadcast", &GameContext_SendBroadcast_Lua)
		.endClass()
		.beginClass<IGameController>("IGameController")
			.addProperty("HealthArmorHudEnabled", &IGameController::IsHealthArmorHudEnabled, &IGameController::SetHealthArmorHudEnabled)
			.addProperty("AmmoHudEnabled", &IGameController::IsAmmoHudEnabled, &IGameController::SetAmmoHudEnabled)

			.addFunction("IsGameOver", &IGameController::IsGameOver)
		.endClass()
		.beginNamespace("Game")
			.addProperty("Server", &CLua::m_pServer, false)
			.addProperty("Context", &CLua::ms_pCGameServer, false)
			// .addFunction("Players", &CGameContext::GetPlayer)
		.endNamespace()

		/// Config.<var_name>
#define MACRO_CONFIG_STR(Name,ScriptName,Len,Def,Save,Desc) \
			.addStaticProperty(#ScriptName, &CConfigProperties::GetConfig_##Name, &CConfigProperties::SetConfig_##Name)

#define MACRO_CONFIG_INT(Name,ScriptName,Def,Min,Max,Save,Desc) \
			.addStaticProperty(#ScriptName, &CConfigProperties::GetConfig_##Name, &CConfigProperties::SetConfig_##Name)

#define MACRO_CONFIG_FLOAT(Name,ScriptName,Def,Min,Max,Save,Desc) \
			.addStaticProperty(#ScriptName, &CConfigProperties::GetConfig_##Name, &CConfigProperties::SetConfig_##Name)

		.beginClass<CConfigProperties>("Config")
			#include <engine/shared/config_variables.h>
		.endClass()

#undef MACRO_CONFIG_STR
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_FLOAT
		// OOP ENDS HERE
	;

	if(g_Config.m_Debug)
		dbg_msg("lua/debug", "registering lua bindings complete (L=%p)", L);
}

#endif // USE_LUA
