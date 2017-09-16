#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "../pb/pb.h"
#include "../pb/codec.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "../lua/pblua.h"
#include "../lua/compat.h"

void test_buffer() {
    const char *str = "111222";
    pb_buffer_t *buf = pb_buffer_new(1024);

    pb_buffer_write(buf, (const uint8_t *) str, strlen(str));
    pb_buffer_swap_last(buf, 4, 2);

    char read[7];
    pb_buffer_read(buf, (uint8_t *) read, strlen(str));
    read[6] = '\0';
    assert(strcmp(read, "221112") == 0);

    pb_buffer_free(buf);
}

void test_encoding() {
    pb_buffer_t *buf = pb_buffer_new(1024);

    uint64_t val = 10;
    varint_encode(buf, val);
    uint64_t val_dec;
    varint_decode(buf, &val_dec, NULL);
    assert(val_dec == val);

    uint32_t val32 = 11133311;
    bit32_encode(buf, val32);
    uint32_t val32_dec;
    bit32_decode(buf, &val32_dec, NULL);
    assert(val32_dec == val32);

    uint64_t val64 = 1113331111111;
    bit64_encode(buf, val64);
    uint64_t val64_dec;
    bit64_decode(buf, &val64_dec, NULL);
    assert(val64_dec == val64);

    uint32_t zig32 = 11133311;
    assert(zig32 == bit32_dezigzag(bit32_zigzag(zig32)));

    uint64_t zig64 = 1113331111111;
    assert(zig64 == bit64_dezigzag(bit64_zigzag(zig64)));
}

void test_lua_call_c(const char *lua_file) {
    lua_State *lstate = luaL_newstate();
    luaL_openlibs(lstate);
    pblua_compat_requiref(lstate, "pblua", luaopen_pblua, 1);
    lua_pop(lstate, 1);

    int fail = luaL_loadfile(lstate, lua_file);
    if (fail) {
        PB_DEBUG_S(lua_tostring(lstate, pb_state_stack_top(0)));
        lua_close(lstate);
        return;
    }
    fail = lua_pcall(lstate, 0, LUA_MULTRET, 0);
    if (fail) {
        PB_DEBUG_S(lua_tostring(lstate, pb_state_stack_top(0)));
        lua_close(lstate);
        return;
    }
    lua_close(lstate);
}

void test_encode_message() {
    test_lua_call_c("test/encode.lua");
}

void test_decode_message() {
    test_lua_call_c("test/decode.lua");
}

int main() {
    test_buffer();
    test_encoding();
    test_encode_message();
    test_decode_message();
    return 0;
}
