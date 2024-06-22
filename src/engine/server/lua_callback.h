#pragma once

#if CONF_LUA

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

int LuaErrorFunc(lua_State *L);

template<class... Args>
bool RunCallback(lua_State *L, const char *pName, Args &&...args)
{
	luabridge::LuaRef callback = luabridge::getGlobal(L, pName);
	if(!callback.isCallable())
		return false;

	return callback.callWithHandler(LuaErrorFunc, std::forward<Args>(args)...).wasOk();
}

#else // CONF_LUA

template<class... Args>
bool RunCallback(lua_State *L, const char *pName, Args &&...args)
{
}

#endif // CONF_LUA
