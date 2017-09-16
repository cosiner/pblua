local int32map = {}
int32map[1] = "1"
int32map[2] = "2"
int32map[4] = "4"
local obj = {
    String = "1" .. "\0" .. "2",
    Strings = { "1" .. "\0" .. "2", "", "1", "1" .. "\0" },
    Sint32 = -1,
    Sint32s = { -1, 1, 0 },
    Sint64 = -1,
    Sint64s = { -1, 1, 0 },
    Int32 = -1,
    Int32s = { -1, 1, 0 },
    Int64 = -1,
    Int64s = { -1, 1, 0 },
    Uint32 = 1,
    Uint32s = { 1, 2, 0 },
    Uint64 = 1,
    Uint64s = { 1, 2, 0 },
    Fixed32 = 1,
    Fixed32s = { 1, 2, 0 },
    Fixed64 = 1,
    Fixed64s = { 1, 2, 0 },
    Sfixed32 = -1,
    Sfixed32s = { -1, 1, 0 },
    Sfixed64 = -1,
    Sfixed64s = { -1, 1, 0 },
    Float = -1.001,
    Floats = { -1.001, 1, 0, 0.0001 },
    Double = -1.001,
    Doubles = { -1.001, 1, 0, 0.0001 },
    Bool = true,
    Bools = { true, false },
    Bytes = "1" .. "\0" .. "2",
    Bytess = { "1" .. "\0" .. "2", "", "1", "1" .. "\0" },
    IntsPacked = { -1, 1, 0 },
    Int32map = int32map,
    Stringmap = { A = "1", B = "2" },
    Msg = { First = "F", Last = "L" },
    Msgs = { {}, { First = "F", Last = "L" }, {} },
    Any = {
        type = "test.User.UserName",
        value = {
            First = "L",
            Last = "F"
        }
    },
    Msgmap = {
        A = { First = "F", Last = "L" },
        B = {}
    }
}

local encode

local escape_char_map = {
    ["\\"] = "\\\\",
    ["\""] = "\\\"",
    ["\b"] = "\\b",
    ["\f"] = "\\f",
    ["\n"] = "\\n",
    ["\r"] = "\\r",
    ["\t"] = "\\t",
}

local escape_char_map_inv = { ["\\/"] = "/" }
for k, v in pairs(escape_char_map) do
    escape_char_map_inv[v] = k
end


local function escape_char(c)
    return escape_char_map[c] or string.format("\\u%04x", c:byte())
end


local function encode_nil(val)
    return "null"
end


local function encode_table(val, stack)
    local res = {}
    stack = stack or {}

    -- Circular reference?
    if stack[val] then error("circular reference") end

    stack[val] = true

    if val[1] ~= nil or next(val) == nil then
        -- Treat as array -- check keys are valid and it is not sparse
        local n = 0
        for k in pairs(val) do
            if type(k) ~= "number" then
                error("invalid table: mixed or invalid key types")
            end
            n = n + 1
        end
        if n ~= #val then
            error("invalid table: sparse array")
        end
        -- Encode
        for i, v in ipairs(val) do
            table.insert(res, encode(v, stack))
        end
        stack[val] = nil
        return "[" .. table.concat(res, ",") .. "]"

    else
        -- Treat as an object
        for k, v in pairs(val) do
            if type(k) ~= "string" then
                error("invalid table: mixed or invalid key types")
            end
            table.insert(res, encode(k, stack) .. ":" .. encode(v, stack))
        end
        stack[val] = nil
        return "{" .. table.concat(res, ",") .. "}"
    end
end

local function encode_string(val)
    return '"' .. val:gsub('[%z\1-\31\\"]', escape_char) .. '"'
end

local function encode_number(val)
    -- Check for NaN, -inf and inf
    if val ~= val or val <= -math.huge or val >= math.huge then
        error("unexpected number value '" .. tostring(val) .. "'")
    end
    return string.format("%.14g", val)
end

local type_func_map = {
    ["nil"] = encode_nil,
    ["table"] = encode_table,
    ["string"] = encode_string,
    ["number"] = encode_number,
    ["boolean"] = tostring,
}

encode = function(val, stack)
    local t = type(val)
    local f = type_func_map[t]
    if f then
        return f(val, stack)
    end
    error("unexpected type '" .. t .. "'")
end

local pb = require('pblua')
local u = pb.loadfile('build/testout/proto.pb')
for i = 0,200000 do
    u:encode('test.User', obj)
end
local content = u:encode('test.User', obj)
local io = require('io')
local fd = io.open('build/testout/pb.encode', 'w')
fd:write(content)
fd:close()