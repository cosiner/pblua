#ifndef PBLUA_COMPAT_H
#define PBLUA_COMPAT_H

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if LUA_VERSION_NUM > 501

#define lua_objlen lua_rawlen
#define pblua_compat_setfuncs(L, reg) luaL_setfuncs((L), (reg), 0)
#define pblua_compat_requiref luaL_requiref

#else

#define lua_Unsigned lua_Integer
#define lua_tounsigned lua_tointeger
#define lua_pushunsigned lua_pushinteger

#define pblua_compat_setfuncs(L, reg) luaL_register((L), NULL, (reg))

void pblua_compat_requiref(lua_State *L, const char *modname,
                           lua_CFunction openf, int glb);

#endif

#endif // PBLUA_COMPAT_H