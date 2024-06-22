#include <exception>
#include <string>

#include <engine/console.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include "lua.h"

IServer * CLua::m_pServer = nullptr;
//CServer * CLua::ms_pCServer = nullptr;
//IGameServer * CLua::ms_pGameServer = nullptr;
CGameContext * CLua::ms_pCGameServer = nullptr;
CLua * CLua::ms_pStaticLua = nullptr;


CLua::CLua()
{
	m_pLuaState = 0;
	CLua::ms_pStaticLua = this;
}

void CLua::SetStaticVars(IServer *pServer, CGameContext *pGameServer)
{
//	CLua::ms_pServer = pServer;
//	CLua::ms_pCServer = (CServer *)pServer;
//	CLua::ms_pGameServer = pGameServer;
	CLua::ms_pCGameServer = pGameServer;
}

void CLua::Init()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pServer = Kernel()->RequestInterface<IServer>();
}

const char *CLua::GetError(int i)
{
	if(i < 0)
		i = ((int)m_lErrors.size() + i);
	if(i > (int)m_lErrors.size()-1)
		return "< error while getting error: given index out of range >";
	return m_lErrors[i].c_str();
}

void CLua::ExecScriptFile(const char *pFileName)
{
	IOHANDLE ScriptFile = Storage()->OpenFile(pFileName, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!ScriptFile)
	{
		dbg_msg("scripting", "unable to open '%s'", pFileName);
		return;
	}

	if(g_Config.m_Debug)
		dbg_msg("lua-script-loader", "loading script '%s'", pFileName);

	char aChunk[1024 * 64];
	int Bytes = io_read(ScriptFile, &aChunk, sizeof(aChunk));
	if(Bytes <= 0)
		return;

	aChunk[Bytes] = '\0';
	ExecScript(aChunk, pFileName);
}

void CLua::ExecScript(const char *pScript, const char *pAsFileName)
{
	if(GetLuaState() == NULL)
	{
		dbg_msg("lua", "error: tried to exec a script without lua state");
		return;
	}

	int Status = luaL_loadbuffer(GetLuaState(), pScript, strlen(pScript), pAsFileName);
	if(Status != 0)
	{
		CLua::ErrorFunc(GetLuaState());
		return;
	}

	Status = lua_pcall(GetLuaState(), 0, LUA_MULTRET, 0);

	if(Status != 0)
	{
		CLua::ErrorFunc(GetLuaState());
	}
}

void CLua::StartupLua()
{
	if(m_pLuaState)
		lua_close(m_pLuaState);

	m_pLuaState = luaL_newstate();

	lua_atpanic(m_pLuaState, CLua::Panic);
	lua_register(m_pLuaState, "errorfunc", CLua::ErrorFunc);

	luaL_openlibs(m_pLuaState);

	dbg_msg("lua", "started");

	RegisterLuaBindings();
}

bool CLua::LoadScript(const char *pPath)
{
	if(GetLuaState() == NULL)
	{
		dbg_msg("lua", "error: tried to load '%s' without lua state", pPath);
		return false;
	}

	if(g_Config.m_Debug)
		dbg_msg("lua-script-loader", "loading script '%s'", pPath);

	int Status = luaL_loadfile(GetLuaState(), pPath);
	if (Status != 0)
	{
		CLua::ErrorFunc(GetLuaState());
		return false;
	}

	Status = lua_pcall(GetLuaState(), 0, LUA_MULTRET, 0);

	if (Status != 0)
	{
		CLua::ErrorFunc(GetLuaState());
		return false;
	}

	return true;
}

bool CLua::HasGlobalCallable(const char *pName)
{
	luabridge::LuaRef ref = luabridge::getGlobal(GetLuaState(), pName);
	return ref.isCallable();
}

/*
int CLua::LoadFolderHelper(const char *pFolder)
{
	dbg_msg("Lua", "Loading Folder '%s'", pFolder);
	CListdirLoadHelper *pParams = new CListdirLoadHelper;
	pParams->m_pSelf = this;

	m_pStorage->ListDirectoryVerbose(IStorage::TYPE_ALL, pFolder, ListdirLoadCallback, pParams);

	int result = pParams->m_NumFiles;
	delete pParams;
	return result;
}*/

int CLua::ListdirLoadCallback(const char *pName, const char *pFullPath, int is_dir, int dir_type, void *pUser)
{
	if(pName[0] == '.')
		return 0;
	if(!is_dir) // these should only be checked for files
	{
		if(str_length(pName) < 5)
		{
			if(g_Config.m_Debug)
				dbg_msg("lua-loader", "skipping '%s' for too short filename", pName);
			return 0;
		}
		if(str_comp_nocase(&pName[str_length(pName) - 4], ".lua") != 0)
		{
			if(g_Config.m_Debug)
				dbg_msg("lua-loader", "skipping '%s' for not ending with '.lua'", pName);
			return 0;
		}
	}

	CListdirLoadHelper *pHelper = (CListdirLoadHelper*)pUser;

	CLua *pSelf = pHelper->m_pSelf;

	//char aFullPath[1024];
	//str_format(aFullPath, sizeof(aFullPath), "%s/%s", pHelper->m_pFullPath, pName);

	if(g_Config.m_Debug)
		dbg_msg("lua", "CLua::ListdirTestCallback(%s, %i, %i, %p) -> %i: %s", pName, is_dir, dir_type, pUser, pHelper->m_NumFiles, pFullPath);
	if(is_dir)
	{
//		pHelper->m_NumFiles += pSelf->LoadFolderHelper(pFullPath);
		// pSelf->m_pStorage->ListDirectoryVerbose(IStorage::TYPE_ALL, pFullPath, ListdirLoadCallback, pUser);
	}
	else
	{
		if(!pSelf->LoadScript(pFullPath))
		{
			dbg_msg("lua", "failed to load '%s'", pFullPath);
		}
		else
		{
			pHelper->m_NumFiles += 1;

			if(g_Config.m_Debug)
				dbg_msg("lua", "loaded '%s'", pFullPath);
		}
	}

	return 0;
}

int CLua::ListdirTestCallback(const char *pName, int is_dir, int dir_type, void *pUser)
{
	if(pName[0] == '.')
		return 0;

	CListdirTestHelper *pHelper = (CListdirTestHelper*)pUser;

	if(g_Config.m_Debug)
		dbg_msg("lua", "(searching '%s') CLua::ListdirTestCallback(%s, %i, %i, %p)", pHelper->m_pSearch, pName, is_dir, dir_type, pUser);
	if(is_dir && str_comp(pName, pHelper->m_pSearch) == 0)
	{
		pHelper->m_Found = true;
		return 1;
	}

	return 0;
}


int CLua::HandleException(std::exception& e)
{
	return HandleException(e.what());
}

int CLua::HandleException(const char *pError)
{
	dbg_msg("lua", "ERROR: %s", pError);

	CLua::ms_pStaticLua->m_lErrors.push_back(std::string(pError));

	// todo something more versatile

	return 0;
}

int CLua::ErrorFunc(lua_State *L)
{
	if (!lua_isstring(L, -1))
		HandleException("unknown error");
	else
		HandleException(lua_tostring(L, -1));

	lua_pop(L, 1); // remove error message
	lua_gc(L, LUA_GCCOLLECT, 0);

	return 0;
}

int CLua::Panic(lua_State *L)
{
	dbg_msg("lua", "!! PANIC !!");
	ErrorFunc(L);

	return 0;
}

ILua *CreateLua() { return new CLua; }
