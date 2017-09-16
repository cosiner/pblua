#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "pb.h"
#include "common.h"

static pb_error_t *messages_new_from_state(pb_state_t *state, pb_message_list_t *msgs) {
    pb_error_t *err = NULL;
    while (!err && pb_state_iter_map_element_pair(state)) {
        int key_index = pb_state_stack_top(-1),
            stack_top = pb_state_stack_top(0);

        pb_string_t key = pb_state_get_string(state, key_index);
        message_t *msg = message_new(key);
        messages_append_msg(msgs, msg);

        pb_state_get_map_element(state, stack_top, string_new("fields"));
        size_t len = pb_state_get_objlen(state, stack_top);
        for (int i = 0; i < len && !err; i++) {
            pb_state_get_array_element(state, stack_top, i);

            uint32_t type = 0;
            uint32_t tag = 0;
            pb_string_t name = {};
            bool repeated = false;
            bool packed = false;
            pb_string_t msg_name = {};
            pb_string_t val_msg_name = {};
            uint32_t key_type = 0;
            uint32_t val_type = 0;
            int count = 0;
            if (pb_state_get_map_element(state, stack_top - count, string_new("type"))) {
                count++;
                type = pb_state_get_uint32(state, stack_top);
            }
            if (pb_state_get_map_element(state, stack_top - count, string_new("tag"))) {
                count++;
                tag = pb_state_get_uint32(state, stack_top);
            }
            if (pb_state_get_map_element(state, stack_top - count, string_new("name"))) {
                count++;
                name = pb_state_get_string(state, stack_top);
            }
            if (pb_state_get_map_element(state, stack_top - count, string_new("repeated"))) {
                count++;
                repeated = pb_state_get_bool(state, stack_top);
            }
            switch (type) {
                case PB_VAL_DOUBLE:
                case PB_VAL_FLOAT:
                case PB_VAL_INT64:
                case PB_VAL_UINT64:
                case PB_VAL_INT32:
                case PB_VAL_FIXED64:
                case PB_VAL_FIXED32:
                case PB_VAL_BOOL:
                case PB_VAL_STRING:
                case PB_VAL_BYTES:
                case PB_VAL_UINT32:
                case PB_VAL_ENUM:
                case PB_VAL_SFIXED32:
                case PB_VAL_SFIXED64:
                case PB_VAL_SINT32:
                case PB_VAL_SINT64:
                    if (pb_state_get_map_element(state, stack_top - count, string_new("packed"))) {
                        count++;
                        packed = pb_state_get_bool(state, stack_top);
                    }
                    err = message_append_field(
                        msg,
                        field_new(name, tag, (pb_valtype_t) type, field_opts_primitive(repeated, packed))
                    );
                    break;
                case PB_VAL_MESSAGE:
                    if (pb_state_get_map_element(state, stack_top - count, string_new("msg_name"))) {
                        count++;
                        msg_name = pb_state_get_string(state, stack_top);
                    }
                    err = message_append_field(
                        msg,
                        field_new(name, tag, (pb_valtype_t) type, field_opts_msg(repeated, msg_name))
                    );
                    break;
                case PB_VAL_ANY:
                    err = message_append_field(
                        msg,
                        field_new(name, tag, (pb_valtype_t) type, field_opts_nop())
                    );
                    break;
                case PB_VAL_MAP:
                    if (pb_state_get_map_element(state, stack_top - count, string_new("val_msg_name"))) {
                        count++;
                        val_msg_name = pb_state_get_string(state, stack_top);
                    }
                    if (pb_state_get_map_element(state, stack_top - count, string_new("key_type"))) {
                        count++;
                        key_type = pb_state_get_uint32(state, stack_top);
                    }
                    if (pb_state_get_map_element(state, stack_top - count, string_new("val_type"))) {
                        count++;
                        val_type = pb_state_get_uint32(state, stack_top);
                    }
                    err = message_append_field(
                        msg,
                        field_new(
                            name,
                            tag,
                            (pb_valtype_t) type,
                            field_opts_map((pb_valtype_t) key_type, (pb_valtype_t) val_type, val_msg_name)
                        )
                    );
                    break;
                default:
                    err = pb_error_new(PB_ERR_VAL_TYPE, "unsupported protobuf type %d", type);
                    break;
            }
            pb_state_popn(state, (size_t) 1 + count);// array element and fields
        }
        pb_state_popn(state, 2); // fields an map value
    }
    return err;
}

pb_error_t *pb_messages_new_descriptor(pb_message_list_t *msgs) {
    void *state_raw = pb_state_new_raw();
    pb_state_t *state = pb_state_new(state_raw);
    pb_error_t *err = pb_state_push_descriptor_meta(state);
    if (err) {
        goto END;
    }
    err = messages_new_from_state(state, msgs);

    END:
    pb_state_free_raw(state_raw);
    pb_state_free(state);
    return err;
}

pb_error_t *pb_messages_parse_pb(pb_message_list_t *desc, pb_buffer_t *buf, pb_message_list_t *msgs) {
    void *raw_state = pb_state_new_raw();
    pb_state_t *state = pb_state_new(raw_state);
    pb_error_t *err = pb_state_push_descriptor_parser(state);
    if (!err) {
        err = pb_decode_message(desc, buf, state, string_new(pb_state_descriptor_type()));
    }
    if (!err) {
        err = pb_state_parse_descriptor(state);
    }
    if (!err) {
        err = messages_new_from_state(state, msgs);
    }

    pb_state_free_raw(raw_state);
    pb_state_free(state);
    return err;
}

static pb_error_t *error_file(const char *fmt, const char *fname) {
    if (!errno) {
        return NULL;
    }
    return pb_error_new(PB_ERR_FAIL, fmt, fname, strerror(errno));
}

pb_error_t *pb_read_file(pb_buffer_t *buf, const char *fname) {
    FILE *fd = fopen(fname, "r");
    if (!fd) {
        return error_file("open file failed %s: %s", fname);
    }

    uint8_t b[256];
    while (!ferror(fd) && !feof(fd)) {
        size_t n = fread(b, 1, sizeof(b), fd);
        pb_buffer_write(buf, b, n);
    }
    pb_error_t *err = NULL;
    if (ferror(fd)) {
        err = error_file("read file failed %s: %s", fname);
    }
    fclose(fd);
    return err;
}

pb_error_t *pb_messages_parse_pbfile(pb_message_list_t *desc, const char *fname, pb_message_list_t *msgs) {
    FILE *fd = fopen(fname, "r");
    if (!fd) {
        return error_file("open file failed %s: %s", fname);
    }

    pb_buffer_t *buf = pb_buffer_new(1024);
    pb_error_t *err = pb_read_file(buf, fname);
    if (!err) {
        err = pb_messages_parse_pb(desc, buf, msgs);
    }
    pb_buffer_free(buf);
    return err;
}
