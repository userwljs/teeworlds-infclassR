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

template<typename ResultType, class... Args>
std::optional<ResultType> RunCallbackWithResult(lua_State *L, const char *pName, Args &&...args)
{
	luabridge::LuaRef callback = luabridge::getGlobal(L, pName);
	if(!callback.isCallable())
		return {};

	luabridge::LuaResult r = callback.callWithHandler(LuaErrorFunc, std::forward<Args>(args)...);
	if(r.size() < 2)
		return {};

	return r[1].cast<ResultType>().valueOr(ResultType());
}

#else // CONF_LUA

template<class... Args>
bool RunCallback(lua_State *L, const char *pName, Args &&...args)
{
	return {};
}

template<typename ResultType, class... Args>
std::optional<ResultType> RunCallbackWithResult(lua_State *L, const char *pName, Args &&...args)
{
	return {};
}

#endif // CONF_LUA
