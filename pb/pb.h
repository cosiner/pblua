#ifndef PB_H
#define PB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG

#include <stdio.h>

#define PB_DEBUG_S(s) printf("%s\n", (s))
#define PB_DEBUG_N(n) printf("%d\n", (n))
#else
#define PB_DEBUG_S(s)
#define PB_DEBUG_N(n)
#endif

/**
 * string
 */
typedef struct pb_string_t pb_string_t;

struct pb_string_t {
    const char *str;
    size_t len;
};

pb_string_t string_new(const char *str);

pb_string_t string_copy(pb_string_t);

void string_free_copy(pb_string_t s);

/**
 * buffer
 */
typedef struct pb_buffer_t pb_buffer_t;

struct pb_buffer_t {
    uint8_t *payload;
    uint64_t cap;

    uint64_t read;
    uint64_t write;
};

pb_buffer_t *pb_buffer_new(size_t cap);

void pb_buffer_readonly(pb_buffer_t *dst, pb_buffer_t *src, size_t size);

void pb_buffer_free(pb_buffer_t *);

size_t pb_buffer_size(pb_buffer_t *);

pb_string_t pb_buffer_payload(pb_buffer_t *buf, size_t len);

size_t pb_buffer_read(pb_buffer_t *, uint8_t *, size_t);

uint8_t *pb_buffer_step_read(pb_buffer_t *buf, size_t n);

size_t pb_buffer_unread(pb_buffer_t *, size_t);

size_t pb_buffer_discard(pb_buffer_t *buf, size_t n);

void pb_buffer_grow(pb_buffer_t *buf, size_t);

size_t pb_buffer_write(pb_buffer_t *, const uint8_t *, size_t);

uint8_t *pb_buffer_step_write(pb_buffer_t *buf, size_t n);

size_t pb_buffer_swap_last(pb_buffer_t *buf, size_t prev_n, size_t last_n);

/**
 * type
 */
typedef enum {
    PB_VAL_DOUBLE = 1,
    PB_VAL_FLOAT = 2,
    PB_VAL_INT64 = 3,
    PB_VAL_UINT64 = 4,
    PB_VAL_INT32 = 5,
    PB_VAL_FIXED64 = 6,
    PB_VAL_FIXED32 = 7,
    PB_VAL_BOOL = 8,
    PB_VAL_STRING = 9,
    PB_VAL_MESSAGE = 11,
    PB_VAL_BYTES = 12,
    PB_VAL_UINT32 = 13,
    PB_VAL_ENUM = 14,
    PB_VAL_SFIXED32 = 15,
    PB_VAL_SFIXED64 = 16,
    PB_VAL_SINT32 = 17,
    PB_VAL_SINT64 = 18,
    PB_VAL_ANY = 100,
    PB_VAL_MAP = 101,
    // PB_VAL_GROUP = 10,
} pb_valtype_t;

#define PB_MAP_KEY_TAG 1
#define PB_MAP_VAL_TAG 2

/**
 * error
 */
typedef enum {
    PB_ERR_MSG_NOT_FOUND = 1,
    PB_ERR_VARINT = 2,
    PB_ERR_STATE_TYPE = 5,
    PB_ERR_VAL_TYPE = 6,
    PB_ERR_WIRE = 7,
    PB_ERR_LENGTH = 8,
    PB_ERR_UNEXPECTED_EOF = 9,
    PB_ERR_FAIL = 10
} pb_errtype_t;

typedef struct pb_error_t {
    pb_errtype_t code;
    char *msg;
    size_t len;
} pb_error_t;

pb_error_t *pb_error_new(pb_errtype_t code, const char *fmt, ...);

void pb_error_free(pb_error_t *);

/**
 * state
 */
typedef struct pb_state_t pb_state_t;

pb_state_t *pb_state_new(void *);

void pb_state_free(pb_state_t *state);

typedef enum pb_statetype_t pb_statetype_t;

enum pb_statetype_t {
    PB_STATE_NIL,
    PB_STATE_NUMBER,
    PB_STATE_BOOLEAN,
    PB_STATE_STRING,
    PB_STATE_OBJECT,
    PB_STATE_OTHER
};

void *pb_state_new_raw();

void pb_state_free_raw(void *s);

int pb_state_stack_top(int);

int pb_state_stack_bottom(int);

int32_t pb_state_get_int32(pb_state_t *, int);

int64_t pb_state_get_int64(pb_state_t *, int);

uint32_t pb_state_get_uint32(pb_state_t *, int);

uint64_t pb_state_get_uint64(pb_state_t *, int);

float pb_state_get_float(pb_state_t *, int);

double pb_state_get_double(pb_state_t *, int);

bool pb_state_get_bool(pb_state_t *, int);

pb_string_t pb_state_get_string(pb_state_t *, int);

size_t pb_state_get_objlen(pb_state_t *, int);

bool pb_state_iter_map_element_pair(pb_state_t *);

void pb_state_get_array_element(pb_state_t *, int sindex, int index);

bool pb_state_get_map_element(pb_state_t *, int sindex, pb_string_t key);

pb_statetype_t pb_state_get_type(pb_state_t *, int);

void pb_state_push_nil(pb_state_t *);

void pb_state_push_int32(pb_state_t *, int32_t);

void pb_state_push_int64(pb_state_t *, int64_t);

void pb_state_push_uint32(pb_state_t *, uint32_t);

void pb_state_push_uint64(pb_state_t *, uint64_t);

void pb_state_push_float(pb_state_t *, float);

void pb_state_push_double(pb_state_t *, double);

void pb_state_push_bool(pb_state_t *, bool);

void pb_state_push_string(pb_state_t *, pb_string_t);

void pb_state_push_array_index(pb_state_t *state, int index);

void pb_state_push_map_key(pb_state_t *state, pb_string_t key);

void pb_state_push_array(pb_state_t *state);

void pb_state_push_map(pb_state_t *state);

void pb_state_append_array_element(pb_state_t *state);

void pb_state_set_map_element(pb_state_t *state);

void pb_state_popn(pb_state_t *state, size_t n);

void pb_state_pop(pb_state_t *);

bool pb_is_state_type_compatible(pb_statetype_t, pb_valtype_t);

pb_error_t *pb_state_push_descriptor_meta(pb_state_t *state);

const char *pb_state_descriptor_type();

pb_error_t *pb_state_push_descriptor_parser(pb_state_t *state);

pb_error_t *pb_state_parse_descriptor(pb_state_t *state);

void pb_state_register_pb_types(pb_state_t *state);

/**
 * message
 */
typedef struct pb_message_list_t pb_message_list_t;

pb_error_t *pb_encode_message(pb_message_list_t *, pb_buffer_t *, pb_state_t *, pb_string_t msg_name);

pb_error_t *pb_decode_message(pb_message_list_t *, pb_buffer_t *, pb_state_t *, pb_string_t msg_name);

pb_error_t *pb_read_file(pb_buffer_t *buf, const char *fname);

pb_error_t *pb_messages_parse_pbfile(pb_message_list_t *desc, const char *fname, pb_message_list_t *msgs);

pb_error_t *pb_messages_parse_pb(pb_message_list_t *desc, pb_buffer_t *buf, pb_message_list_t *msgs);

pb_error_t *pb_messages_new_descriptor(pb_message_list_t *msgs);

#endif // PB_H