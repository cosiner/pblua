#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../pb/pb.h"
#include "compat.h"
#include "luafile_gen.h"

struct pb_state_t {
    lua_State *state;
    bool first_key_pushed;
};

inline void *pb_state_new_raw() {
    return luaL_newstate();
}

inline void pb_state_free_raw(void *s) {
    lua_close(s);
}

inline pb_state_t *pb_state_new(void *state) {
    pb_state_t *s = (pb_state_t *) calloc(1, sizeof(pb_state_t));
    s->state = (lua_State *) state;
    return s;
}

inline void pb_state_free(pb_state_t *state) {
    free(state);
}

inline int pb_state_stack_top(int offset) {
    return -1 + offset;
}

inline int pb_state_stack_bottom(int offset) {
    return 1 + offset;
}

inline int32_t pb_state_get_int32(pb_state_t *state, int sindex) {
    return (int32_t) lua_tointeger(state->state, sindex);
}

inline int64_t pb_state_get_int64(pb_state_t *state, int sindex) {
    return (int64_t) lua_tointeger(state->state, sindex);
}

inline uint32_t pb_state_get_uint32(pb_state_t *state, int sindex) {
    return (uint32_t) lua_tounsigned(state->state, sindex);
}

inline uint64_t pb_state_get_uint64(pb_state_t *state, int sindex) {
    return (uint64_t) lua_tounsigned(state->state, sindex);
}

inline float pb_state_get_float(pb_state_t *state, int sindex) {
    return (float) lua_tonumber(state->state, sindex);
}

inline double pb_state_get_double(pb_state_t *state, int sindex) {
    return (double) lua_tonumber(state->state, sindex);
}

inline bool pb_state_get_bool(pb_state_t *state, int sindex) {
    return (bool) lua_toboolean(state->state, sindex);
}

inline pb_string_t pb_state_get_string(pb_state_t *state, int sindex) {
    pb_string_t str;
    str.str = lua_tolstring(state->state, sindex, &str.len);
    return str;
}

inline size_t pb_state_get_objlen(pb_state_t *state, int sindex) {
    return lua_objlen(state->state, sindex);
}

bool pb_state_iter_map_element_pair(pb_state_t *state) {
    if (!state->first_key_pushed) {
        state->first_key_pushed = true;
        lua_pushnil(state->state);
    }
    if (!lua_next(state->state, pb_state_stack_top(-1))) {
        state->first_key_pushed = false;
        return false;
    }
    return true;
}

inline static bool state_pop_if_nil(pb_state_t *state) {
    if (lua_isnil(state->state, pb_state_stack_top(0))) {
        pb_state_pop(state);
        return false;
    }
    return true;
}

inline void pb_state_get_array_element(pb_state_t *state, int sindex, int index) {
    // lua is 1-index based.
    pb_state_push_array_index(state, index);
    lua_rawget(state->state, sindex - 1);
}

inline bool pb_state_get_map_element(pb_state_t *state, int sindex, pb_string_t key) {
    pb_state_push_map_key(state, key);
    lua_gettable(state->state, sindex - 1);
    return state_pop_if_nil(state);
}

pb_statetype_t pb_state_get_type(pb_state_t *state, int sindex) {
    switch (lua_type(state->state, sindex)) {
        case LUA_TNIL:
            return PB_STATE_NIL;
        case LUA_TNUMBER:
            return PB_STATE_NUMBER;
        case LUA_TSTRING:
            return PB_STATE_STRING;
        case LUA_TBOOLEAN:
            return PB_STATE_BOOLEAN;
        case LUA_TTABLE:
            return PB_STATE_OBJECT;
        default:
            return PB_STATE_OTHER;
    }
}

inline void pb_state_push_nil(pb_state_t *state) {
    lua_pushnil(state->state);
}

inline void pb_state_push_int32(pb_state_t *state, int32_t n) {
    lua_pushinteger(state->state, (lua_Integer) n);
}

inline void pb_state_push_int64(pb_state_t *state, int64_t n) {
    lua_pushinteger(state->state, (lua_Integer) n);
}

inline void pb_state_push_uint32(pb_state_t *state, uint32_t n) {
    lua_pushunsigned(state->state, (lua_Unsigned) n);
}

inline void pb_state_push_uint64(pb_state_t *state, uint64_t n) {
    lua_pushunsigned(state->state, (lua_Unsigned) n);
}

inline void pb_state_push_float(pb_state_t *state, float f) {
    lua_pushnumber(state->state, (lua_Number) f);
}

inline void pb_state_push_double(pb_state_t *state, double d) {
    lua_pushnumber(state->state, (lua_Number) d);
}

inline void pb_state_push_bool(pb_state_t *state, bool b) {
    lua_pushboolean(state->state, (int) b);
}

inline void pb_state_push_string(pb_state_t *state, pb_string_t s) {
    lua_pushlstring(state->state, s.str, s.len);
}

inline void pb_state_push_array(pb_state_t *state) {
    lua_newtable(state->state);
}

inline void pb_state_push_map(pb_state_t *state) {
    lua_newtable(state->state);
}

inline void pb_state_push_array_index(pb_state_t *state, int index) {
    pb_state_push_int32(state, index + 1);
}

inline void pb_state_push_map_key(pb_state_t *state, pb_string_t key) {
    pb_state_push_string(state, key);
}

inline void pb_state_append_array_element(pb_state_t *state) {
    lua_settable(state->state, pb_state_stack_top(-2));
}

inline void pb_state_set_map_element(pb_state_t *state) {
    lua_settable(state->state, pb_state_stack_top(-2));
}

inline void pb_state_popn(pb_state_t *state, size_t n) {
    lua_pop(state->state, n);
}

inline void pb_state_pop(pb_state_t *state) {
    pb_state_popn(state, 1);
}

bool pb_is_state_type_compatible(pb_statetype_t t, pb_valtype_t v) {
    switch (t) {
        case PB_STATE_NUMBER:
        case PB_STATE_BOOLEAN:
            return v == PB_VAL_SINT32 ||
                   v == PB_VAL_SINT64 ||
                   v == PB_VAL_INT32 ||
                   v == PB_VAL_INT64 ||
                   v == PB_VAL_UINT32 ||
                   v == PB_VAL_UINT64 ||
                   v == PB_VAL_FIXED32 ||
                   v == PB_VAL_FIXED64 ||
                   v == PB_VAL_SFIXED32 ||
                   v == PB_VAL_SFIXED64 ||
                   v == PB_VAL_FLOAT ||
                   v == PB_VAL_DOUBLE ||
                   v == PB_VAL_BOOL ||
                   v == PB_VAL_ENUM;
        case PB_STATE_STRING:
            return v == PB_VAL_STRING ||
                   v == PB_VAL_BYTES;
        case PB_STATE_OBJECT:
            return v == PB_VAL_MAP ||
                   v == PB_VAL_MESSAGE ||
                   v == PB_VAL_ANY;
        case PB_STATE_NIL:
        case PB_STATE_OTHER:
        default:
            return false;
    }
}

inline static const char *state_error(pb_state_t *state) {
    return lua_tostring(state->state, -1);
}

pb_error_t *pb_state_push_descriptor_meta(pb_state_t *state) {
    pb_state_push_map(state);
    pb_state_register_pb_types(state);
    lua_setglobal(state->state, "pbtypes");

    int err = luaL_loadbuffer(state->state, (const char *) lua_desc_lua, lua_desc_lua_len, NULL) ||
              lua_pcall(state->state, 0, LUA_MULTRET, 0);
    if (err) {
        return pb_error_new(PB_ERR_FAIL, "load FileDescriptorSet failed: %s", state_error(state));
    }
    lua_getglobal(state->state, "descriptor");
    return NULL;
}

inline const char *pb_state_descriptor_type() {
    return "FileDescriptorSet";
}

pb_error_t *pb_state_push_descriptor_parser(pb_state_t *state) {
    luaL_openlibs(state->state);
    pb_state_push_map(state);
    pb_state_register_pb_types(state);
    lua_setglobal(state->state, "pbtypes");

    int err = luaL_loadbuffer(state->state, (const char *) lua_parse_lua, lua_parse_lua_len, NULL) ||
              lua_pcall(state->state, 0, LUA_MULTRET, 0);
    if (err) {
        return pb_error_new(PB_ERR_FAIL, "load descriptor parser failed: %s", state_error(state));
    }
    lua_getglobal(state->state, "parse");
    return NULL;
}

pb_error_t *pb_state_parse_descriptor(pb_state_t *state) {
    int err = lua_pcall(state->state, 1, LUA_MULTRET, 0);
    if (err) {
        return pb_error_new(PB_ERR_FAIL, "parse descriptor failed: %s", state_error(state));
    }
    return NULL;
}

void pb_state_register_pb_types(pb_state_t *state) {
    typedef struct type_reg {
        const char *name;
        pb_valtype_t type;
    } type_reg;
    type_reg types[] = {
        {"PB_VAL_DOUBLE",   PB_VAL_DOUBLE},
        {"PB_VAL_FLOAT",    PB_VAL_FLOAT},
        {"PB_VAL_INT64",    PB_VAL_INT64},
        {"PB_VAL_UINT64",   PB_VAL_UINT64},
        {"PB_VAL_INT32",    PB_VAL_INT32},
        {"PB_VAL_FIXED64",  PB_VAL_FIXED64},
        {"PB_VAL_FIXED32",  PB_VAL_FIXED32},
        {"PB_VAL_BOOL",     PB_VAL_BOOL},
        {"PB_VAL_STRING",   PB_VAL_STRING},
        {"PB_VAL_MESSAGE",  PB_VAL_MESSAGE},
        {"PB_VAL_BYTES",    PB_VAL_BYTES},
        {"PB_VAL_UINT32",   PB_VAL_UINT32},
        {"PB_VAL_ENUM",     PB_VAL_ENUM},
        {"PB_VAL_SFIXED32", PB_VAL_SFIXED32},
        {"PB_VAL_SFIXED64", PB_VAL_SFIXED64},
        {"PB_VAL_SINT32",   PB_VAL_SINT32},
        {"PB_VAL_SINT64",   PB_VAL_SINT64},
        {"PB_VAL_ANY",      PB_VAL_ANY},
        {"PB_VAL_MAP",      PB_VAL_MAP},
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(type_reg); i++) {
        pb_state_push_string(state, string_new(types[i].name));
        pb_state_push_uint32(state, (uint32_t) types[i].type);
        pb_state_set_map_element(state);
    }
}