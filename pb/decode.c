#include <string.h>
#include "pb.h"
#include "common.h"
#include "codec.h"

static void push_default(pb_message_list_t *msgs, pb_state_t *s, field_t *field) {
    switch (field->field_wire) {
        case WIRE_LENGTH_DELIMITED:
            switch (field->type) {
                case PB_VAL_STRING:
                case PB_VAL_BYTES:
                    pb_state_push_string(s, string_new(""));
                    break;
                case PB_VAL_MESSAGE:
                    pb_state_push_map(s);
                    message_t *msg = messages_find(msgs, field->opts.msg.name);
                    if (!msg) {
                        break;
                    }
                    for (field_t *curr = msg->first; curr; curr = curr->next) {
                        pb_state_push_string(s, curr->name);
                        push_default(msgs, s, curr);
                        pb_state_set_map_element(s);
                    }
                    break;
                case PB_VAL_ANY:
                    pb_state_push_nil(s);
                    break;
                default:
                    // packed
                    pb_state_push_array(s);
                    break;
            }
            break;
        case WIRE_REPEATED:
            switch (field->type) {
                case PB_VAL_MAP:
                    pb_state_push_map(s);
                    break;
                default:
                    pb_state_push_array(s);
                    break;
            }
            break;
        default:
            switch (field->type) {
                case PB_VAL_SINT32:
                case PB_VAL_INT32:
                case PB_VAL_ENUM:
                case PB_VAL_SFIXED32:
                    pb_state_push_int32(s, 0);
                    break;
                case PB_VAL_UINT32:
                case PB_VAL_FIXED32:
                    pb_state_push_uint32(s, 0);
                    break;
                case PB_VAL_SINT64:
                case PB_VAL_INT64:
                case PB_VAL_SFIXED64:
                    pb_state_push_int64(s, 0);
                    break;
                case PB_VAL_UINT64:
                case PB_VAL_FIXED64:
                    pb_state_push_uint64(s, 0);
                    break;
                case PB_VAL_FLOAT:
                    pb_state_push_float(s, 0);
                    break;
                case PB_VAL_DOUBLE:
                    pb_state_push_double(s, 0);
                    break;
                case PB_VAL_BOOL:
                    pb_state_push_bool(s, false);
                    break;
                default:
                    pb_state_push_int32(s, 0);
                    break;
            }
    }
}

static pb_error_t *read_header(pb_buffer_t *buf, header_t *hdr, size_t *size) {
    pb_error_t *err = varint_decode(buf, &hdr->tag, size);
    if (err) {
        return err;
    }
    hdr->wire = (uint8_t) (hdr->tag & HEADER_WIRE_MASK);
    hdr->tag >>= HEADER_WIRE_BITCOUNT;
    if (hdr->wire == WIRE_LENGTH_DELIMITED) {
        err = varint_decode(buf, &hdr->len, size);
        if (err) {
            return err;
        }
    }
    return NULL;
}

static pb_error_t *read_string_ignore_wire(pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h, size_t *size) {
    uint8_t *payload = pb_buffer_step_read(buf, h->len);
    if (!payload) {
        return pb_error_new(PB_ERR_UNEXPECTED_EOF, "unexpected EOF");
    }
    pb_string_t str = {
        .str=(const char *) payload,
        .len=h->len
    };
    if (size) {
        *size += str.len;
    }
    pb_state_push_string(s, str);
    return NULL;
}

static pb_error_t *read_string(pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h, size_t *size) {
    if (field->field_wire != h->wire) {
        return pb_error_new(
            PB_ERR_WIRE,
            "invalid wire for field %s, expect %s, got %s",
            field->name.str,
            wire_name(field->field_wire),
            wire_name((wire_t) h->wire)
        );
    }
    return read_string_ignore_wire(buf, s, field, h, size);
}

static pb_error_t *read_number_ignore_wire(pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h, size_t *size) {
    union {
        uint32_t u32;
        uint64_t u64;
    } p;
    memset(&p, 0, sizeof(p));
    pb_error_t *err = NULL;

    wire_t w = (wire_t) h->wire;
    if (w == WIRE_LENGTH_DELIMITED) {
        w = field->value_wire;
    }
    switch (w) {
        case WIRE_VARINT:
            err = varint_decode(buf, &p.u64, size);
            break;
        case WIRE_BIT32:
            err = bit32_decode(buf, &p.u32, size);
            break;
        case WIRE_BIT64:
            err = bit64_decode(buf, &p.u64, size);
            break;
        default:;
            err = pb_error_new(
                PB_ERR_WIRE,
                "invalid wire for field %s, expect Varint/Bit32/Bit64, got %s",
                wire_name(w)
            );
            break;
    }
    if (err) {
        return err;
    }
    switch (field->type) {
        case PB_VAL_SINT32:
            p.u64 = (uint64_t) bit32_dezigzag((int32_t) p.u64);
        case PB_VAL_INT32:
        case PB_VAL_SFIXED32:
            pb_state_push_int32(s, (int32_t) p.u64);
            break;
        case PB_VAL_ENUM:
        case PB_VAL_UINT32:
        case PB_VAL_FIXED32:
            pb_state_push_uint32(s, (uint32_t) p.u64);
            break;
        case PB_VAL_BOOL:
            pb_state_push_bool(s, p.u64 > 0);
            break;
        case PB_VAL_SINT64:
            p.u64 = (uint64_t) bit64_dezigzag((int64_t) p.u64);
        case PB_VAL_INT64:
        case PB_VAL_SFIXED64:
            pb_state_push_int64(s, (int64_t) p.u64);
            break;
        case PB_VAL_UINT64:
        case PB_VAL_FIXED64:
            pb_state_push_uint64(s, p.u64);
            break;
        case PB_VAL_FLOAT:
            pb_state_push_float(s, uint32_to_float(p.u32));
            break;
        case PB_VAL_DOUBLE:
            pb_state_push_double(s, uint64_to_double(p.u64));
            break;
        default:
            return pb_error_new(
                PB_ERR_FAIL,
                "internal error: invalid field type: %s, %d",
                field->name.str,
                field->type
            );
    }
    return NULL;
}

static pb_error_t *read_packed_number(pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h, size_t *size) {
    if (field->field_wire != h->wire) {
        return pb_error_new(
            PB_ERR_WIRE,
            "invalid wire for field: %s, expect %s, got %s",
            field->name,
            wire_name(field->field_wire),
            wire_name((wire_t) h->wire)
        );
    }
    return read_number_ignore_wire(buf, s, field, h, size);
}

static pb_error_t *read_number(pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h, size_t *size) {
    if (field->value_wire != h->wire) {
        return pb_error_new(
            PB_ERR_WIRE,
            "invalid wire for field: %s, expect %s, got %s",
            field->name,
            wire_name(field->value_wire),
            wire_name((wire_t) h->wire)
        );
    }
    return read_number_ignore_wire(buf, s, field, h, size);
}

static pb_error_t *decode_packed(pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h) {
    pb_error_t *err = NULL;
    size_t n = 0;
    while (1) {
        if (n == h->len) {
            break;
        }
        if (n > h->len) {
            return pb_error_new(PB_ERR_LENGTH, "invalid length for field %s", field->name.str);
        }
        pb_state_push_array_index(s, (int) pb_state_get_objlen(s, pb_state_stack_top(0)));
        err = read_packed_number(buf, s, field, h, &n);
        if (err) {
            pb_state_pop(s);
            break;
        }
        pb_state_append_array_element(s);
    }
    return err;
}

static pb_error_t *decode_all(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h);

static pb_error_t *
decode_custom_message_no_header(pb_message_list_t *msgs, message_t *msg, pb_buffer_t *buf, pb_state_t *s, size_t len);

static pb_error_t *
decode_custom_message_no_header_by_name(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, pb_string_t msg_name,
                                        size_t len);

static pb_error_t *decode_any(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h) {
    size_t size = pb_buffer_size(buf);
    header_t h_typ = {};
    pb_error_t *err = read_header(buf, &h_typ, NULL);
    if (err) {
        return err;
    }
    err = read_string(buf, s, field, &h_typ, NULL);
    if (err) {
        return err;
    }
    pb_string_t msg_name = pb_state_get_string(s, pb_state_stack_top(0));
    pb_state_pop(s);

    message_t *msg = messages_find(msgs, msg_name);
    if (!msg) {
        // ignore
        push_default(msgs, s, field);

        size_t read = size - pb_buffer_size(buf);
        if (read > h->len) {
            return pb_error_new(PB_ERR_LENGTH, "invalid length for field: %s", field->name.str);
        }
        pb_buffer_discard(buf, h->len - read);
        return NULL;
    }

    header_t h_val = {};
    err = read_header(buf, &h_val, NULL);
    if (err) {
        return err;
    }
    return decode_custom_message_no_header(msgs, msg, buf, s, h_val.len);
}

static pb_error_t *
decode_length_delimited(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h) {
    switch (field->type) {
        case PB_VAL_STRING:
        case PB_VAL_BYTES:
            return read_string(buf, s, field, h, NULL);
        case PB_VAL_MESSAGE:
            return decode_custom_message_no_header_by_name(msgs, buf, s, field->opts.msg.name, h->len);
        case PB_VAL_ANY:
            return decode_any(msgs, buf, s, field, h);
        default:
            return decode_packed(buf, s, field, h);
    }
}

static pb_error_t *
decode_repeated(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h) {
    pb_error_t *err = NULL;
    if (field->type == PB_VAL_MAP) {
        if (!field->map_key || !field->map_val) {
            goto MAP_END;
        }

        header_t h_key = {};
        pb_buffer_t nbuf;
        pb_buffer_readonly(&nbuf, buf, h->len);

        err = read_header(&nbuf, &h_key, NULL);
        if (err) {
            goto MAP_END;
        }
        switch (field->map_key->field_wire) {
            case WIRE_LENGTH_DELIMITED:
                err = read_string(&nbuf, s, field->map_key, &h_key, NULL);
                break;
            default:
                err = read_number(&nbuf, s, field->map_key, &h_key, NULL);
                break;
        }
        if (err) {
            goto MAP_END;
        }

        if (pb_buffer_size(&nbuf) == 0) {
            push_default(msgs, s, field->map_val);
        } else {
            header_t h_val = {};
            err = read_header(&nbuf, &h_val, NULL);
            if (!err) {
                err = decode_all(msgs, &nbuf, s, field->map_val, &h_val);
            }
            if (err) {
                pb_state_pop(s);
                goto MAP_END;
            }
        }

        MAP_END:
        if (!err) {
            pb_buffer_discard(buf, h->len);
            pb_state_set_map_element(s);
        }
    } else if (field->array_element) {
        pb_state_push_array_index(s, (int) pb_state_get_objlen(s, pb_state_stack_top(0)));
        err = decode_all(msgs, buf, s, field->array_element, h);
        if (err) {
            pb_state_pop(s);
        } else {
            pb_state_append_array_element(s);
        }
    }
    return err;
}

static pb_error_t *decode_all(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h) {
    switch (field->field_wire) {
        case WIRE_LENGTH_DELIMITED:
            return decode_length_delimited(msgs, buf, s, field, h);
        case WIRE_REPEATED:
            return decode_repeated(msgs, buf, s, field, h);
        case WIRE_VARINT:
        case WIRE_BIT32:
        case WIRE_BIT64:
            return read_number(buf, s, field, h, NULL);
        default:
            return pb_error_new(PB_ERR_FAIL, "internal error, invalid wire for field %s", field->name.str);
    }
}

static bool field_is_packed(field_t *field) {
    if (field->field_wire != WIRE_LENGTH_DELIMITED) {
        return false;
    }
    switch (field->type) {
        case PB_VAL_ANY:
        case PB_VAL_MESSAGE:
        case PB_VAL_STRING:
        case PB_VAL_BYTES:
            return false;
        default:
            return field->opts.primitive.packed;
    }
}

static pb_error_t *
decode_message_field(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, header_t *h) {
    pb_state_push_string(s, field->name);
    bool is_repeated = field->field_wire == WIRE_REPEATED || field_is_packed(field);
    if (is_repeated) {
        if (!pb_state_get_map_element(s, pb_state_stack_top(-1), field->name)) {
            if (field->type == PB_VAL_MAP) {
                pb_state_push_map(s);
            } else {
                pb_state_push_array(s);
            }
        }
    }
    pb_error_t *err = decode_all(msgs, buf, s, field, h);
    if (err) {
        if (is_repeated) {
            pb_state_pop(s);
        }
        pb_state_pop(s);
        return err;
    }

    pb_state_set_map_element(s);
    return NULL;
}

typedef struct tag_node_t tag_node_t;

struct tag_node_t {
    uint64_t tag;
    tag_node_t *next;
};

typedef struct tag_list_t {
    tag_node_t *head;
    tag_node_t *tail;
} tag_list_t;

static tag_list_t *tags_new() {
    return calloc(1, sizeof(tag_list_t));
}

static tag_node_t *tags_node_new(tag_node_t *next, uint64_t tag) {
    tag_node_t *n = malloc(sizeof(tag_node_t));
    n->next = next;
    n->tag = tag;
    return n;
}

static void tags_free(tag_list_t *tags) {
    tag_node_t *curr = tags->head, *tmp;
    while (curr) {
        tmp = curr->next;
        free(curr);
        curr = tmp;
    }
    free(tags);
}

static bool tags_append(tag_list_t *tags, uint64_t tag) {
    tag_node_t *tail = tags->tail;
    if (!tail) {
        tags->head = tags->tail = tags_node_new(NULL, tag);
        return true;
    }
    if (tail->tag == tag) {
        return false;
    }
    tags->tail = tail->next = tags_node_new(tail->next, tag);
    return true;
}

static bool tags_remove(tag_list_t *tags, uint64_t tag) {
    tag_node_t *prev = NULL, *curr = tags->head;
    while (curr) {
        if (curr->tag == tag) {
            tag_node_t *tmp = curr->next;
            free(curr);
            if (!prev) {
                tags->head = tmp;
            } else {
                prev->next = tmp;
            }
            if (!tmp) {
                tags->tail = prev;
            }
            return true;
        }

        prev = curr;
        curr = curr->next;
    }
    return false;
}

static pb_error_t *
decode_custom_message_no_header(pb_message_list_t *msgs, message_t *msg, pb_buffer_t *buf, pb_state_t *s, size_t len) {
    pb_state_push_map(s);
    pb_error_t *err = NULL;
    tag_list_t *tags = tags_new();
    size_t appeared = 0;

    pb_buffer_t nbuf;
    pb_buffer_readonly(&nbuf, buf, len);

    field_t *currField = NULL;
    while (pb_buffer_size(&nbuf) > 0) {
        header_t h = {};
        err = read_header(&nbuf, &h, NULL);
        if (err) {
            break;
        }
        currField = message_find_field_by_tag(msg, currField, h.tag);
        if (!currField) {
            pb_buffer_discard(&nbuf, h.len);
            continue;
        }
        err = decode_message_field(msgs, &nbuf, s, currField, &h);
        if (err) {
            break;
        }
        if (tags_append(tags, h.tag)) {
            appeared++;
        }
    }
    if (err) {
        pb_state_pop(s);
        tags_free(tags);
        return err;
    }
    pb_buffer_discard(buf, len);
    if (1) {
        field_t *curr = msg->first;
        while (curr) {
            if (!tags_remove(tags, curr->tag)) {
                pb_state_push_string(s, curr->name);
                push_default(msgs, s, curr);
                pb_state_set_map_element(s);
            }
            curr = curr->next;
        }
    }
    tags_free(tags);
    return NULL;
}

static pb_error_t *
decode_custom_message_no_header_by_name(
    pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, pb_string_t msg_name, size_t len) {
    message_t *msg = messages_find(msgs, msg_name);
    if (!msg) {
        return pb_error_new(PB_ERR_MSG_NOT_FOUND, "message not found: %s", msg_name.str);
    }
    return decode_custom_message_no_header(msgs, msg, buf, s, len);
}

pb_error_t *pb_decode_message(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, pb_string_t msg_name) {
    return decode_custom_message_no_header_by_name(msgs, buf, s, msg_name, pb_buffer_size(buf));
}