function clean_msg(msg)
    local m = {
        fields = {}
    }
    for _, field in ipairs(msg.field) do
        local f = {
            type = field.type,
            tag = field.number,
            repeated = field.label == 3, -- repeated
            packed = false,
            name = field.name,
            msg_name = string.gsub(field.type_name, '^%.', ''),
        }
        -- enum
        if f.type == pbtypes.PB_VAL_ENUM then
            f.msg_name = ''
        end
        for _, opt in ipairs(field.options) do
            if f.packed then
                break
            end
            f.packed = opt.packed
        end

        m.fields[#m.fields + 1] = f
    end
    return m
end

function clean_map_type(m)
    local is = #m.fields == 2
    if not is then
        return false
    end
    local field1, field2 = m.fields[1].name, m.fields[2].name
    local swapped = false
    if field1 == 'value' then
        field1, field2 = field2, field1
        swapped = true
    end
    is = field1 == 'key' and field2 == 'value'
    if is and swapped then
        m.fields[1], m.fields[2] = m.fields[2], m.fields[1]
    end
    return is
end

function parse_types(data, ctx, types)
    for _, t in ipairs(types) do
        local name = ctx .. t.name
        local m = clean_msg(t)
        if clean_map_type(m) then
            data.maps[name] = m
        else
            data.msgs[name] = m
        end
        parse_types(data, name .. '.', t.nested_type)
    end
end

function clean_field_types(msgs, maps, any_msg_name)
    for _, m in pairs(msgs) do
        for _, f in ipairs(m.fields) do
            if f.msg_name == any_msg_name then
                f.type = pbtypes.PB_VAL_ANY -- any
                f.msg_name = ''
            elseif f.msg_name ~= '' then
                local map = maps[f.msg_name]
                if map then
                    f.type = 101 -- map
                    f.msg_name = ''

                    local key = map.fields[1]
                    local val = map.fields[2]
                    f.key_type = key.type
                    f.val_type = val.type
                    f.val_msg_name = val.msg_name
                end
            end
        end
    end
end
function parse(obj)
    local data = {
        msgs = {},
        maps = {}
    }
    for _, file in ipairs(obj.file) do
        parse_types(data, file.package ~= '' and file.package .. '.' or file.package, file.message_type)
    end
    clean_field_types(data.maps, data.maps, 'google.protobuf.Any')
    clean_field_types(data.msgs, data.maps, 'google.protobuf.Any')
    return data.msgs
end