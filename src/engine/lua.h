#ifndef ENGINE_LUA_H
#define ENGINE_LUA_H

#include "kernel.h"

class ILua : public IInterface
{
	MACRO_INTERFACE("lua")
protected:
	class lua_State *m_pLuaState{};

public:
	virtual class lua_State *GetLuaState() { return m_pLuaState; }

	virtual void Init() = 0;
	virtual void StartupLua() = 0;
	virtual const char *GetError(int i) = 0;
	virtual int NumErrors() const = 0;

	virtual void ExecScriptFile(const char *pFileName) = 0;
	virtual void ExecScript(const char *pScript, const char *pAsFileName = nullptr) = 0;

	virtual bool HasGlobalCallable(const char *pName) = 0;
};

extern ILua *CreateLua();

#endif
