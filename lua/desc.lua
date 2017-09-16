descriptor = {}

descriptor['FileDescriptorSet'] = {
    fields = {
        {
            name = "file",
            tag = 1,
            type = pbtypes.PB_VAL_MESSAGE,
            repeated = true,
            msg_name = 'FileDescriptorProto'
        }
    }
}

descriptor['FileDescriptorProto'] = {
    fields = {
        {
            name = "name",
            tag = 1,
            type = pbtypes.PB_VAL_STRING
        },
        {
            name = "package",
            tag = 2,
            type = pbtypes.PB_VAL_STRING
        },
        {
            name = "dependency",
            tag = 3,
            type = pbtypes.PB_VAL_STRING,
            repeated = true
        },
        {
            name = "message_type",
            tag = 4,
            type = pbtypes.PB_VAL_MESSAGE,
            repeated = true,
            msg_name = 'DescriptorProto'
        },
        {
            name = "syntax",
            tag = 12,
            type = pbtypes.PB_VAL_STRING
        }
    }
}

descriptor['DescriptorProto'] = {
    fields = {
        {
            name = "name",
            tag = 1,
            type = pbtypes.PB_VAL_STRING
        },
        {
            name = "field",
            tag = 2,
            type = pbtypes.PB_VAL_MESSAGE,
            repeated = true,
            msg_name = 'FieldDescriptorProto'
        },
        {
            name = "nested_type",
            tag = 3,
            type = pbtypes.PB_VAL_MESSAGE,
            repeated = true,
            msg_name = 'DescriptorProto'
        }
    }
}

descriptor['FieldDescriptorProto'] = {
    fields = {
        {
            name = "name",
            tag = 1,
            type = pbtypes.PB_VAL_STRING
        },
        {
            name = "number",
            tag = 3,
            type = pbtypes.PB_VAL_INT32
        },
        {
            name = "label",
            tag = 4,
            type = pbtypes.PB_VAL_ENUM
        },
        {
            name = "type",
            tag = 5,
            type = pbtypes.PB_VAL_ENUM
        },
        {
            name = "type_name",
            tag = 6,
            type = pbtypes.PB_VAL_STRING
        },
        {
            name = "options",
            tag = 8,
            type = pbtypes.PB_VAL_MESSAGE,
            repeated = true,
            msg_name = 'FieldOptions'
        }
    }
}

descriptor['FieldOptions'] = {
    fields = {
        {
            name = "packed",
            tag = 2,
            type = pbtypes.PB_VAL_BOOL
        }
    }
}
