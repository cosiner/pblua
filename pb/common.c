#include <string.h>
#include <printf.h>
#include "pb.h"
#include "common.h"

const char *wire_name(wire_t w) {
    switch (w) {
        case WIRE_VARINT:
            return "Varint";
        case WIRE_LENGTH_DELIMITED:
            return "LengthDelimited";
        case WIRE_BIT32:
            return "Bit32";
        case WIRE_BIT64:
            return "Bit64";
        default:
            return "Unknown";
    }
}

wire_t value_wire_type(pb_valtype_t t) {
    switch (t) {
        case PB_VAL_SINT32:
        case PB_VAL_SINT64:
        case PB_VAL_INT32:
        case PB_VAL_INT64:
        case PB_VAL_UINT32:
        case PB_VAL_UINT64:
        case PB_VAL_BOOL:
        case PB_VAL_ENUM:
            return WIRE_VARINT;
        case PB_VAL_FIXED32:
        case PB_VAL_SFIXED32:
        case PB_VAL_FLOAT:
            return WIRE_BIT32;
        case PB_VAL_FIXED64:
        case PB_VAL_DOUBLE:
        case PB_VAL_SFIXED64:
            return WIRE_BIT64;
        case PB_VAL_MAP:
        case PB_VAL_STRING:
        case PB_VAL_BYTES:
        case PB_VAL_MESSAGE:
        case PB_VAL_ANY:
        default:
            return WIRE_LENGTH_DELIMITED;
    }
}

wire_t field_wire_type(field_t *field) {
    switch (field->value_wire) {
        case WIRE_LENGTH_DELIMITED:
            switch (field->type) {
                case PB_VAL_MAP:
                    return WIRE_REPEATED;
                case PB_VAL_MESSAGE:
                    if (field->opts.msg.repeated) {
                        return WIRE_REPEATED;
                    }
                    break;
                case PB_VAL_STRING:
                case PB_VAL_BYTES:
                    if (field->opts.primitive.repeated) {
                        return WIRE_REPEATED;
                    }
                default:;
            }
            return field->value_wire;
        default:
            if (field->opts.primitive.repeated) {
                if (field->opts.primitive.packed) {
                    return WIRE_LENGTH_DELIMITED;
                }
                return WIRE_REPEATED;
            }
            return field->value_wire;
    }
}

static const char *strrchr_n(const char *ptr, char ch, size_t len) {
    for (size_t i = len; i > 0; i--) {
        if (*(ptr + i - 1) == ch) {
            return ptr + i - 1;
        }
    }
    return NULL;
}

message_t *messages_find(pb_message_list_t *msgs, pb_string_t name) {
    const char *ptr = strrchr_n(name.str, '/', name.len);
    if (ptr) {
        name.len -= (ptr - name.str);
        name.str = ptr;
    }
    message_t *curr = msgs->first;
    while (curr) {
        size_t max;
        if (name.len > curr->name.len) {
            max = name.len;
        } else {
            max = curr->name.len;
        }
        if (strncmp(curr->name.str, name.str, max) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

field_t *message_find_field_by_tag(message_t *msg, field_t *prev, uint64_t tag) {
    if (!prev) {
        prev = msg->first;
    }
    field_t *curr = prev;
    while (curr) {
        if (curr->tag == tag) {
            return curr;
        }
        curr = curr->next;
    }
    curr = msg->first;
    while (curr && curr != prev) {
        if (curr->tag == tag) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}


static void field_init(field_t *field, pb_string_t name, uint64_t tag, pb_valtype_t type, field_opts_t opts) {
    field->name = name;
    field->tag = tag;
    field->type = type;
    field->opts = opts;

    field->value_wire = value_wire_type(field->type);
    field->field_wire = field_wire_type(field);

    switch (field->type) {
        case PB_VAL_MAP:
            switch (field->opts.map.value_type) {
                case PB_VAL_MAP:
                case PB_VAL_ANY:
                case PB_VAL_MESSAGE:
                    opts = field_opts_msg(false, opts.map.value_message_name);
                    break;
                default:
                    opts = field_opts_primitive(false, false);
                    break;
            }
            field->map_val = field_new(
                string_new(""),
                PB_MAP_VAL_TAG,
                field->opts.map.value_type,
                opts
            );
            field->map_key = field_new(
                string_new(""),
                PB_MAP_KEY_TAG,
                field->opts.map.key_type,
                field_opts_primitive(false, false)
            );
        case PB_VAL_ANY:
        case PB_VAL_MESSAGE:
            // custom message
            if (field->opts.msg.repeated) {
                field->array_element = field_new(
                    string_copy(field->opts.msg.name),
                    field->tag,
                    field->type,
                    field_opts_msg(false, field->opts.msg.name)
                );
            }
            break;
        default:
            // primitive
            if (field->opts.primitive.repeated) {
                field->array_element = field_new(
                    string_copy(field->opts.msg.name),
                    field->tag,
                    field->type,
                    field_opts_primitive(false, false)
                );
            }
            break;
    }
}

field_t *field_new(pb_string_t name, uint64_t tag, pb_valtype_t type, field_opts_t opts) {
    field_t *field = calloc(1, sizeof(field_t));
    field_init(field, string_copy(name), tag, type, opts);
    return field;
}

void field_free(field_t *field) {
    if (field->array_element) {
        field_free(field->array_element);
    }
    if (field->map_key) {
        field_free(field->map_key);
    }
    if (field->map_val) {
        field_free(field->map_val);
    }
    free(field);
}

message_t *message_new(pb_string_t name) {
    message_t *msg = calloc(1, sizeof(message_t));
    msg->name = string_copy(name);
    return msg;
}

static void message_free(message_t *msg) {
    field_t *field = msg->first,
        *field_tmp;
    while (field) {
        field_tmp = field->next;
        switch (field->type) {
            case PB_VAL_MAP:
                string_free_copy(field->opts.map.value_message_name);
                break;
            case PB_VAL_MESSAGE:
                string_free_copy(field->opts.msg.name);
                break;
            default:;
        }
        string_free_copy(field->name);
        field_free(field);
        field = field_tmp;
    }

    message_t *msg_tmp;
    while (msg) {
        msg_tmp = msg->next;
        string_free_copy(msg->name);
        msg = msg_tmp;
    }
}

pb_message_list_t *messages_new() {
    pb_message_list_t *msgs = calloc(1, sizeof(pb_message_list_t));
    msgs->any_type_field = string_new("type");
    msgs->any_value_field = string_new("value");
    return msgs;
}

void messages_free(pb_message_list_t *msgs) {
    if (msgs->first) {
        message_free(msgs->first);
    }
    free(msgs);
}

field_opts_t field_opts_nop() {
    field_opts_t opts = {};
    return opts;
}

field_opts_t field_opts_map(pb_valtype_t key_type, pb_valtype_t value_type, pb_string_t val_msg_name) {
    field_opts_t opts = {};
    opts.map.key_type = key_type;
    opts.map.value_type = value_type;
    opts.map.value_message_name = string_copy(val_msg_name);
    return opts;
}

static field_opts_t field_opts_msg_no_copy(bool repeated, pb_string_t msg_name) {
    field_opts_t opts = {};
    opts.msg.repeated = repeated;
    opts.msg.name = msg_name;
    return opts;
}

field_opts_t field_opts_msg(bool repeated, pb_string_t msg_name) {
    return field_opts_msg_no_copy(repeated, string_copy(msg_name));
}

field_opts_t field_opts_primitive(bool repeated, bool packed) {
    field_opts_t opts = {};
    opts.primitive.repeated = repeated;
    opts.primitive.packed = packed;
    return opts;
}

void messages_append_msg(pb_message_list_t *msgs, message_t *msg) {
    if (!msgs->first) {
        msgs->first = msg;
        return;
    }
    message_t *parent = msgs->first;
    while (parent->next) {
        parent = parent->next;
    }
    parent->next = msg;
}

pb_error_t *message_append_field(message_t *msg, field_t *field) {
    field_t *prev = NULL,
        *curr = msg->first;
    if (!curr) {
        msg->first = field;
        return NULL;
    }
    while (curr) {
        if (curr->tag == field->tag) {
            return pb_error_new(
                PB_ERR_FAIL,
                "duplicate field tag: %s, %s, %d",
                msg->name.str,
                field->name.str,
                field->tag
            );
        }
        if (curr->tag > field->tag) {
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    field->next = curr;
    if (!prev) {
        msg->first = field;
    } else {
        prev->next = field;
    }
    return NULL;
}
