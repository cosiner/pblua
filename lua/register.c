#include <lua.h>
#include <lauxlib.h>
#include "../pb/pb.h"
#include "../pb/common.h"
#include "compat.h"

#define PBLUA_METATABLE "PBLua"
#define PBLUA_DESC_OBJ "PBLuaDesc"

void pblua_new_userdata(lua_State *state, pb_message_list_t *msg) {
    pb_message_list_t **userdata = (pb_message_list_t **) lua_newuserdata(state, sizeof(pb_message_list_t *));
    *userdata = msg;

    luaL_getmetatable(state, PBLUA_METATABLE);
    lua_setmetatable(state, pb_state_stack_top(-1));
}

static pb_message_list_t *pblua_check(lua_State *state, int index) {
    pb_message_list_t **userdata = (pb_message_list_t **) luaL_checkudata(state, index, PBLUA_METATABLE);
    if (!userdata) {
        luaL_error(state, "invalid arguments");
        return NULL;
    }
    return *userdata;
}

static void pblua_push_and_free_error(lua_State *state, pb_error_t *err) {
    lua_pushstring(state, err->msg);
    pb_error_free(err);
}

static int pblua_load_buffer(lua_State *state, pb_buffer_t *buf, pb_error_t *err) {
    pb_message_list_t *msgs = NULL;
    if (!err) {
        lua_getfield(state, LUA_REGISTRYINDEX, PBLUA_DESC_OBJ);
        pb_message_list_t *desc = pblua_check(state, pb_state_stack_top(0));
        lua_pop(state, 1);

        msgs = messages_new();
        err = pb_messages_parse_pb(desc, buf, msgs);
    }

    int ret = 1;
    if (err) {
        if (msgs) {
            messages_free(msgs);
        }
        lua_pushnil(state);
        pblua_push_and_free_error(state, err);
        ret++;
    } else {
        pblua_new_userdata(state, msgs);
    }
    return ret;
}

static int pblua_load_file(lua_State *state) {
    const char *fname = lua_tostring(state, pb_state_stack_top(0));
    pb_buffer_t *buf = pb_buffer_new(1024);
    pb_error_t *err = pb_read_file(buf, fname);

    int ret = pblua_load_buffer(state, buf, err);
    pb_buffer_free(buf);
    return ret;
}

static int pblua_load_string(lua_State *state) {
    pb_buffer_t *buf = pb_buffer_new(1024);
    size_t len = 0;
    const char *pbcontent = lua_tolstring(state, pb_state_stack_top(0), &len);
    pb_buffer_write(buf, (const uint8_t *) pbcontent, len);

    int ret = pblua_load_buffer(state, buf, NULL);
    pb_buffer_free(buf);
    return ret;
}

static int pblua_encode(lua_State *state) {
    pb_message_list_t *msg = pblua_check(state, pb_state_stack_bottom(0));
    pb_buffer_t *buf = pb_buffer_new(1024);
    pb_state_t *s = pb_state_new(state);
    pb_error_t *err = pb_encode_message(msg, buf, s, pb_state_get_string(s, pb_state_stack_top(-1)));
    int ret = 1;
    if (err) {
        lua_pushnil(state);
        pblua_push_and_free_error(state, err);
        ret++;
    } else {
        pb_state_push_string(s, pb_buffer_payload(buf, pb_buffer_size(buf)));
    }
    pb_buffer_free(buf);
    pb_state_free(s);
    return ret;
}

static int pblua_decode(lua_State *state) {
    pb_message_list_t *msg = pblua_check(state, pb_state_stack_bottom(0));
    pb_buffer_t *buf = pb_buffer_new(1024);
    pb_state_t *s = pb_state_new(state);
    pb_string_t msg_name = pb_state_get_string(s, pb_state_stack_top(-1));
    pb_string_t data = pb_state_get_string(s, pb_state_stack_top(0));
    pb_buffer_write(buf, (const uint8_t *) data.str, data.len);

    pb_error_t *err = pb_decode_message(msg, buf, s, msg_name);
    int ret = 1;
    if (err) {
        lua_pushnil(state);
        pblua_push_and_free_error(state, err);
        ret++;
    }
    pb_buffer_free(buf);
    pb_state_free(s);
    return ret;
}

static int pblua_free(lua_State *state) {
    pb_message_list_t *msg = pblua_check(state, pb_state_stack_bottom(0));
    messages_free(msg);
    return 0;
}
//
//static int pblua_index(lua_State *state) {
//    const char *name = lua_tostring(state, pb_state_stack_top(0));
//    luaL_getmetatable(state, PBLUA_METATABLE);
//    lua_getfield(state, pb_state_stack_top(0), name);
//    if (!lua_isnil(state, pb_state_stack_top(0))) {
//        return 1;
//    }
//    lua_pop(state, 2);// pop metatable and the nil field
//    lua_pushnil(state);
//    return 1;
//}

int luaopen_pblua(lua_State *state) {
    luaL_newmetatable(state, PBLUA_METATABLE);
    luaL_Reg meta[] = {
        {"encode", pblua_encode},
        {"decode", pblua_decode},
        {"__gc",   pblua_free},
//        {"__index", pblua_index},
        {NULL, NULL}
    };
    pblua_compat_setfuncs(state, meta);
    lua_pushvalue(state, pb_state_stack_top(0));
    lua_setfield(state, pb_state_stack_top(-1), "__index");

    pb_message_list_t *desc = messages_new();
    pb_error_t *err = pb_messages_new_descriptor(desc);

    int ret = 1;
    if (err) {
        lua_pushnil(state);
        pblua_push_and_free_error(state, err);
        ret++;
        return ret;
    }
    pblua_new_userdata(state, desc);
    lua_setfield(state, LUA_REGISTRYINDEX, PBLUA_DESC_OBJ);

    luaL_Reg lib[] = {
        {"loadfile",   pblua_load_file},
        {"loadstring", pblua_load_string},
        {NULL, NULL}
    };
    pblua_compat_newlib(state, "pblua", lib);

    pb_state_t *s = pb_state_new(state);
    pb_state_register_pb_types(s);
    pb_state_free(s);
    return ret;
}