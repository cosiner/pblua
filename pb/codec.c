#include "pb.h"
#include "codec.h"

#define VARINT_BITCOUNT 7
#define VARINT_FLAG_MASK (1 << VARINT_BITCOUNT)
#define VARINT_VALUE_MASK (VARINT_FLAG_MASK - 1)

size_t varint_bytecount(uint64_t n) {
    size_t c = 0;
    do {
        c++;
        n >>= VARINT_BITCOUNT;
    } while (n);
    return c;
}

size_t varint_encode(pb_buffer_t *buf, uint64_t n) {
    size_t c = varint_bytecount(n);
    uint8_t *payload = pb_buffer_step_write(buf, c);
    do {
        uint8_t b = (uint8_t) (n & VARINT_VALUE_MASK);
        n >>= VARINT_BITCOUNT;
        if (n > 0) {
            b |= VARINT_FLAG_MASK;
        }
        *payload++ = b;
    } while (n);
    return c;
}

pb_error_t *varint_decode(pb_buffer_t *buf, uint64_t *n, size_t *size) {
    uint64_t val = 0;
    uint8_t *p;
    uint8_t b;
    uint8_t shift = 0;
    size_t c = pb_buffer_size(buf);
    while (shift < 64 && (p = pb_buffer_step_read(buf, 1))) {
        b = *p;
        val += ((uint64_t) b & VARINT_VALUE_MASK) << shift;

        if (!(b & VARINT_FLAG_MASK)) {
            if (size) {
                *size += c - pb_buffer_size(buf);
            }
            *n = val;
            return NULL;
        }
        shift += VARINT_BITCOUNT;
    }
    if (shift >= 64) {
        return pb_error_new(PB_ERR_VARINT, "varint overflow 64bit");
    }
    return pb_error_new(PB_ERR_UNEXPECTED_EOF, "unexpected EOF");
}

#define BIT32_BYTECOUNT 4

inline size_t bit32_encode(pb_buffer_t *buf, uint32_t n) {
    uint8_t *payload = pb_buffer_step_write(buf, BIT32_BYTECOUNT);
    for (int i = 0; i < BIT32_BYTECOUNT; i++) {
        payload[i] = (uint8_t) (n & 0xff);
        n >>= 8;
    }
    return BIT32_BYTECOUNT;
}

pb_error_t *bit32_decode(pb_buffer_t *buf, uint32_t *n, size_t *size) {
    uint8_t *payload = pb_buffer_step_read(buf, BIT32_BYTECOUNT);
    if (!payload) {
        return pb_error_new(PB_ERR_UNEXPECTED_EOF, "unexpected EOF");
    }
    uint32_t v = 0;
    for (int i = 0; i < BIT32_BYTECOUNT; i++) {
        v += (uint32_t) (payload[i]) << (i * 8);
    }
    if (size) {
        *size += BIT32_BYTECOUNT;
    }
    *n = v;
    return NULL;
}

#define BIT64_BYTECOUNT 8

inline size_t bit64_encode(pb_buffer_t *buf, uint64_t n) {
    uint8_t *payload = pb_buffer_step_write(buf, BIT64_BYTECOUNT);
    for (int i = 0; i < BIT64_BYTECOUNT; i++) {
        payload[i] = (uint8_t) (n & 0xff);
        n >>= 8;
    }
    return BIT64_BYTECOUNT;
}

pb_error_t *bit64_decode(pb_buffer_t *buf, uint64_t *n, size_t *size) {
    uint8_t *payload = pb_buffer_step_read(buf, BIT64_BYTECOUNT);
    if (!payload) {
        return pb_error_new(PB_ERR_UNEXPECTED_EOF, "unexpected EOF");
    }
    uint64_t v = 0;
    for (int i = 0; i < BIT64_BYTECOUNT; i++) {
        v += (uint64_t) (payload[i]) << (i * 8);
    }
    if (size) {
        *size += BIT64_BYTECOUNT;
    }
    *n = v;
    return NULL;
}

int32_t bit32_zigzag(int32_t n) {
    return (n << 1) ^ (n >> 31);
}

int32_t bit32_dezigzag(int32_t n) {
    return (n >> 1) ^ (-(n & 1));
}

int64_t bit64_zigzag(int64_t n) {
    return (n << 1) ^ (n >> 63);
}

int64_t bit64_dezigzag(int64_t n) {
    return (n >> 1) ^ (-(n & 1));
}

typedef union {
    float f;
    uint32_t u;
} float_uint32_t;

typedef union {
    double d;
    uint64_t u;
} double_uint64_t;

uint32_t float_to_uint32(float f) {
    float_uint32_t v;
    v.f = f;
    return v.u;
}

float uint32_to_float(uint32_t u) {
    float_uint32_t v;
    v.u = u;
    return v.f;
}

uint64_t double_to_uint64(double f) {
    double_uint64_t v;
    v.d = f;
    return v.u;
}

double uint64_to_double(uint64_t u) {
    double_uint64_t v;
    v.u = u;
    return v.d;
}