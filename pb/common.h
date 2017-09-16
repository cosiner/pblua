#ifndef PB_COMMON_H
#define PB_COMMON_H

#include "stdbool.h"
#include "pb.h"

#define HEADER_WIRE_BITCOUNT  3
#define HEADER_WIRE_MASK ((1 << HEADER_WIRE_BITCOUNT) - 1)

typedef enum {
    WIRE_VARINT = 0,
    WIRE_BIT64 = 1,
    WIRE_BIT32 = 5,
    WIRE_LENGTH_DELIMITED = 2,

    WIRE_REPEATED = 255
} wire_t;

typedef struct {
    uint64_t tag;
    uint8_t wire;
    uint64_t len;
} header_t;

typedef union {
    struct {
        pb_valtype_t key_type;
        pb_valtype_t value_type;
        pb_string_t value_message_name;
    } map;
    struct {
        bool repeated;
        bool packed;
    } primitive;
    struct {
        bool repeated;
        pb_string_t name;
    } msg;
} field_opts_t;

typedef struct field_t field_t;
struct field_t {
    pb_string_t name;
    uint64_t tag;

    pb_valtype_t type;
    wire_t value_wire;
    field_opts_t opts;
    wire_t field_wire;

    field_t *array_element;
    field_t *map_key;
    field_t *map_val;

    field_t *next;
};

typedef struct message_t {
    pb_string_t name;
    field_t *first;

    struct message_t *next;
} message_t;

struct pb_message_list_t {
    message_t *first;

    pb_string_t any_type_field;
    pb_string_t any_value_field;
};

const char *wire_name(wire_t w);

wire_t value_wire_type(pb_valtype_t);

wire_t field_wire_type(field_t *);

pb_message_list_t *messages_new();

void messages_free(pb_message_list_t *msgs);

message_t *message_new(pb_string_t);

field_opts_t field_opts_nop();

field_opts_t field_opts_map(pb_valtype_t key_type, pb_valtype_t value_type, pb_string_t val_msg_name);

field_opts_t field_opts_msg(bool repeated, pb_string_t msg_name);

field_opts_t field_opts_primitive(bool repeated, bool packed);

field_t *field_new(pb_string_t name, uint64_t tag, pb_valtype_t type, field_opts_t opts);

void messages_append_msg(pb_message_list_t *, message_t *);

pb_error_t *message_append_field(message_t *msg, field_t *field);

message_t *messages_find(pb_message_list_t *, pb_string_t name);

field_t *message_find_field_by_tag(message_t *msg, field_t *prev, uint64_t tag);

#endif // PB_COMMON_H