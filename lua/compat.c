#include "compat.h"

void pblua_compat_requiref(lua_State *L, const char *modname,
                           lua_CFunction openf, int glb) {
    lua_pushcfunction(L, openf);
    lua_pushstring(L, modname);
    lua_call(L, 1, 1);
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_replace(L, -2);
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, modname);
    lua_pop(L, 1);
    if (glb) {
        lua_pushvalue(L, -1);
        lua_setglobal(L, modname);
    }
}
