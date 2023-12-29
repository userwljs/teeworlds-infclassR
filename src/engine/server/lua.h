#ifndef ENGINE_SERVER_LUA_H
#define ENGINE_SERVER_LUA_H

#include <exception>
#include <vector>
#include <string>

#include <engine/lua.h>

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

class CGameContext;
class CServer;
class IConsole;
class IServer;
class IStorage;

class CLua : public ILua
{
	IStorage *m_pStorage = nullptr;
	IConsole *m_pConsole = nullptr;

	static CLua *ms_pStaticLua;

public:
//	static IServer *ms_pServer;
//	static CServer *ms_pCServer;
//	static IGameServer *ms_pGameServer;
	static CGameContext *ms_pCGameServer;

private:
	std::vector<std::string> m_lErrors;

protected:
	class IStorage *Storage() { return m_pStorage; }
	class IConsole *Console() { return m_pConsole; }

public:
	CLua();

	void Init() override;
	void StartupLua() override;
	const char *GetError(int i) override;
	int NumErrors() const override { return m_lErrors.size(); }

	void ExecScriptFile(const char *pFileName) override;
	void ExecScript(const char *pScript, const char *pAsFileName = nullptr) override;

	bool LoadScript(const char *pPath);

	static int ListdirLoadCallback(const char *pName, const char *pFullPath, int is_dir, int dir_type, void *pUser);
	static int ListdirTestCallback(const char *pName, int is_dir, int dir_type, void *pUser);

	static void SetStaticVars(IServer *pServer, CGameContext *pGameServer);

	static int HandleException(std::exception& e);
	static int HandleException(const char *pError);
	static int ErrorFunc(lua_State *L);
	static int Panic(lua_State *L);

	static IServer *m_pServer;

private:
//	int LoadFolderHelper(const char *pFolder);

	void RegisterLuaBindings();

	// helper stuff
	struct CListdirLoadHelper
	{
		CLua *m_pSelf{};
		unsigned m_NumFiles{};
	};

	struct CListdirTestHelper
	{
		const char *m_pSearch{};
		bool m_Found{};
	};
};

#endif
