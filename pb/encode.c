#include <string.h>
#include "pb.h"
#include "common.h"
#include "codec.h"

static size_t write_header(pb_buffer_t *buf, header_t *hdr) {
    size_t n = 0;
    n += varint_encode(buf, hdr->tag << 3 | (uint64_t) hdr->wire);
    if (hdr->wire == WIRE_LENGTH_DELIMITED) {
        n += varint_encode(buf, hdr->len);
    }
    return n;
}

static size_t write_header_swap_last(pb_buffer_t *buf, header_t *h, bool must) {
    if (!must && h->wire == WIRE_LENGTH_DELIMITED && h->len == 0) {
        return 0;
    }
    size_t n = write_header(buf, h);
    pb_buffer_swap_last(buf, h->len, n);
    return n;
}

static size_t write_raw_string(pb_buffer_t *buf, pb_string_t str, field_t *field, bool must) {
    if (!must && str.len == 0) {
        return 0;
    }

    header_t h;
    h.tag = field->tag;
    h.wire = field->value_wire;
    h.len = str.len;

    size_t n = write_header(buf, &h);
    n += pb_buffer_write(buf, (const uint8_t *) str.str, str.len);
    return n;
}

static size_t write_string(pb_buffer_t *buf, pb_state_t *s, int sindex, field_t *field, bool must) {
    return write_raw_string(buf, pb_state_get_string(s, sindex), field, must);
}

static size_t write_number(pb_buffer_t *buf, pb_state_t *s, int sindex, field_t *field, bool must) {
    union {
        uint32_t u32;
        uint64_t u64;
    } p;
    memset(&p, 0, sizeof(p));
    switch (field->type) {
        case PB_VAL_SINT32:
        case PB_VAL_INT32:
        case PB_VAL_SFIXED32:
            p.u32 = (uint32_t) pb_state_get_int32(s, sindex);
            break;
        case PB_VAL_SINT64:
        case PB_VAL_INT64:
        case PB_VAL_SFIXED64:
            p.u64 = (uint64_t) pb_state_get_int64(s, sindex);
            break;
        case PB_VAL_UINT32:
        case PB_VAL_FIXED32:
            p.u32 = pb_state_get_uint32(s, sindex);
            break;
        case PB_VAL_UINT64:
        case PB_VAL_FIXED64:
            p.u64 = pb_state_get_uint64(s, sindex);
            break;
        case PB_VAL_FLOAT:
            p.u32 = float_to_uint32(pb_state_get_float(s, sindex));
            break;
        case PB_VAL_DOUBLE:
            p.u64 = double_to_uint64(pb_state_get_double(s, sindex));
            break;
        case PB_VAL_BOOL:
            p.u32 = (uint32_t) pb_state_get_bool(s, sindex);
            break;
        case PB_VAL_ENUM:
            p.u32 = pb_state_get_uint32(s, sindex);
            break;
        default:;
    }
    if (!must && p.u32 == 0 && p.u64 == 0) {
        return 0;
    }

    size_t n = 0;
    if (field->field_wire != WIRE_LENGTH_DELIMITED) {
        header_t h = {};
        h.tag = field->tag;
        h.wire = field->value_wire;
        n += write_header(buf, &h);
    }
    switch (field->type) {
        case PB_VAL_SINT32:
            p.u32 = (uint32_t) bit32_zigzag((int32_t) p.u32);
        case PB_VAL_INT32:
        case PB_VAL_UINT32:
        case PB_VAL_BOOL:
        case PB_VAL_ENUM:
            n += varint_encode(buf, (uint64_t) p.u32);
            break;
        case PB_VAL_SINT64:
            p.u64 = (uint64_t) bit64_zigzag((int64_t) p.u64);
        case PB_VAL_INT64:
        case PB_VAL_UINT64:
            n += varint_encode(buf, p.u64);
            break;
        case PB_VAL_FIXED32:
        case PB_VAL_SFIXED32:
        case PB_VAL_FLOAT:
            n += bit32_encode(buf, p.u32);
            break;
        case PB_VAL_FIXED64:
        case PB_VAL_SFIXED64:
        case PB_VAL_DOUBLE:
            n += bit64_encode(buf, p.u64);
            break;
        default:;
    }
    return n;
}

static pb_error_t *encode_all(pb_message_list_t *, pb_buffer_t *, pb_state_t *, field_t *, bool must);

static pb_error_t *encode_message_field(pb_message_list_t *, pb_buffer_t *, pb_state_t *, field_t *);

static pb_error_t *
encode_custom_message_no_header(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, pb_string_t msg_name);

static pb_error_t *encode_repeated(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field) {
    pb_error_t *err = NULL;
    if (field->type == PB_VAL_MAP) {
        if (field->map_key && field->map_val) {
            header_t h = {};
            h.tag = field->tag;
            h.wire = field->value_wire;
            int key_index = pb_state_stack_top(-1),
                value_index = pb_state_stack_top(0);
            while (pb_state_iter_map_element_pair(s) && !err) {
                pb_statetype_t key_type = pb_state_get_type(s, key_index);
                pb_statetype_t val_type = pb_state_get_type(s, value_index);
                if (pb_is_state_type_compatible(key_type, field->map_key->type) &&
                    pb_is_state_type_compatible(val_type, field->map_val->type)) {

                    h.len = pb_buffer_size(buf);
                    if (key_type == PB_STATE_STRING) {
                        write_string(buf, s, key_index, field->map_key, true);
                    } else {
                        write_number(buf, s, key_index, field->map_key, true);
                    }
                    err = encode_all(msgs, buf, s, field->map_val, true);
                    if (!err) {
                        h.len = pb_buffer_size(buf) - h.len;
                        write_header_swap_last(buf, &h, true);
                    }
                }
                pb_state_pop(s);
            }
        }
    } else if (field->array_element) {
        size_t len = pb_state_get_objlen(s, pb_state_stack_top(0));
        if (len > 0) {
            for (size_t i = 0; i < len && !err; i++) {
                pb_state_get_array_element(s, pb_state_stack_top(0), (int) i);
                err = encode_all(msgs, buf, s, field->array_element, true);
                pb_state_pop(s);
            }
        }
    }
    return err;
}

static pb_error_t *encode_packed(pb_buffer_t *buf, pb_state_t *s, field_t *field, bool must) {
    size_t len = pb_state_get_objlen(s, pb_state_stack_top(0));
    if (!must && len == 0) {
        return NULL;
    }
    header_t h = {};
    h.tag = field->tag;
    h.wire = field->field_wire;
    for (size_t i = 0; i < len; i++) {
        pb_state_get_array_element(s, pb_state_stack_top(0), (int) i);
        h.len += write_number(buf, s, pb_state_stack_top(0), field, true);
        pb_state_pop(s);
    }
    write_header_swap_last(buf, &h, must);
    return NULL;
}

static pb_error_t *encode_any(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, bool must) {
    header_t h_any = {};
    h_any.wire = field->value_wire;
    h_any.tag = field->tag;
    h_any.len = pb_buffer_size(buf);

    if (!pb_state_get_map_element(s, pb_state_stack_top(0), msgs->any_type_field)) {
        return NULL;
    }
    pb_error_t *err = NULL;
    pb_string_t str = pb_state_get_string(s, pb_state_stack_top(0));
    if (!messages_find(msgs, str)) {
        err = pb_error_new(PB_ERR_MSG_NOT_FOUND, "message not found: %s", str.str);
    }
    if (err || !pb_state_get_map_element(s, pb_state_stack_top(-1), msgs->any_value_field)) {
        goto END;
    }
    field_t tmp = {.tag=1, .value_wire=WIRE_LENGTH_DELIMITED};
    write_raw_string(buf, str, &tmp, true);

    header_t h = {};
    h.tag = 2;
    h.wire = field->value_wire;
    h.len = pb_buffer_size(buf);
    err = encode_custom_message_no_header(msgs, buf, s, str);
    h.len = pb_buffer_size(buf) - h.len;
    if (!err) {
        write_header_swap_last(buf, &h, false);
    }
    pb_state_pop(s); // pop value

    END:
    pb_state_pop(s); // pop type
    if (!err) {
        h_any.len = pb_buffer_size(buf) - h_any.len;
        write_header_swap_last(buf, &h_any, must);
    }
    return err;
}

static pb_error_t *
encode_custom_message(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, bool must) {
    header_t h = {};
    h.tag = field->tag;
    h.wire = field->value_wire;
    h.len = pb_buffer_size(buf);
    pb_error_t *err = encode_custom_message_no_header(msgs, buf, s, field->opts.msg.name);
    if (err) {
        return err;
    }
    h.len = pb_buffer_size(buf) - h.len;
    write_header_swap_last(buf, &h, must);
    return NULL;
}

static pb_error_t *
encode_length_delimited(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, bool must) {
    switch (field->type) {
        case PB_VAL_MESSAGE:
            return encode_custom_message(msgs, buf, s, field, must);
        case PB_VAL_ANY:
            return encode_any(msgs, buf, s, field, must);
        case PB_VAL_STRING:
        case PB_VAL_BYTES:
            write_string(buf, s, pb_state_stack_top(0), field, must);
            return NULL;
        default:
            return encode_packed(buf, s, field, must);
    }
}

static pb_error_t *encode_all(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field, bool must) {
    switch (field->field_wire) {
        case WIRE_REPEATED:
            return encode_repeated(msgs, buf, s, field);
        case WIRE_LENGTH_DELIMITED:
            return encode_length_delimited(msgs, buf, s, field, must);
        default:
            write_number(buf, s, pb_state_stack_top(0), field, must);
            return NULL;
    }
}

static pb_error_t *encode_message_field(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, field_t *field) {
    if (!pb_state_get_map_element(s, pb_state_stack_top(0), field->name)) {
        return NULL;
    }
    pb_error_t *err = encode_all(msgs, buf, s, field, false);
    pb_state_pop(s);
    return err;
}

static pb_error_t *
encode_custom_message_no_header(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, pb_string_t msg_name) {
    message_t *msg = messages_find(msgs, msg_name);
    if (!msg) {
        return pb_error_new(PB_ERR_MSG_NOT_FOUND, "message not found: %s", msg_name.str);
    }
    switch (pb_state_get_type(s, pb_state_stack_top(0))) {
        case PB_STATE_NIL:
            return NULL;
        case PB_STATE_OBJECT:
            break;
        default:
            return pb_error_new(PB_ERR_STATE_TYPE, "invalid value type to encode");
    }

    pb_error_t *err = NULL;
    field_t *curr = msg->first;
    while (curr && !err) {
        err = encode_message_field(msgs, buf, s, curr);
        curr = curr->next;
    }

    return err;
}

pb_error_t *pb_encode_message(pb_message_list_t *msgs, pb_buffer_t *buf, pb_state_t *s, pb_string_t msg_name) {
    pb_error_t *err = encode_custom_message_no_header(msgs, buf, s, msg_name);
    return err;
}