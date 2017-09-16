#ifndef PB_CODEC_H
#define PB_CODEC_H

#include "pb.h"

size_t varint_encode(pb_buffer_t *, uint64_t);

pb_error_t *varint_decode(pb_buffer_t *, uint64_t *, size_t *);

size_t bit32_encode(pb_buffer_t *, uint32_t);

pb_error_t *bit32_decode(pb_buffer_t *, uint32_t *, size_t *);

size_t bit64_encode(pb_buffer_t *, uint64_t);

pb_error_t *bit64_decode(pb_buffer_t *, uint64_t *, size_t *);

int32_t bit32_zigzag(int32_t);

int32_t bit32_dezigzag(int32_t);

int64_t bit64_zigzag(int64_t);

int64_t bit64_dezigzag(int64_t);

uint32_t float_to_uint32(float);

float uint32_to_float(uint32_t);

uint64_t double_to_uint64(double);

double uint64_to_double(uint64_t);

#endif // PB_CODEC_H
