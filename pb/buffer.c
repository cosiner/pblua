#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include "pb.h"

inline pb_string_t string_new(const char *str) {
    pb_string_t s = {
        .str=str,
        .len=strlen(str)
    };
    return s;
}

pb_string_t string_copy(pb_string_t s) {
    if (!s.str || s.len == 0) {
        return s;
    }
    char *ns = malloc(s.len);
    memcpy(ns, s.str, s.len);
    pb_string_t n = {
        .str=ns,
        .len=s.len
    };
    return n;
}

inline void string_free_copy(pb_string_t s) {
    if (s.str && s.len > 0) {
        free((void *) s.str);
    }
}

pb_error_t *pb_error_new(pb_errtype_t code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *buf = malloc(256);
    size_t len = vsnprintf(buf, 256, fmt, args);
    va_end(args);

    pb_error_t *err = malloc(sizeof(pb_error_t));
    err->msg = buf;
    err->len = len;
    err->code = code;
    return err;
}

inline void pb_error_free(pb_error_t *err) {
    if (err) {
        if (err->msg) {
            free(err->msg);
        }
        free(err);
    }
}

pb_buffer_t *pb_buffer_new(size_t n) {
    const size_t DEFAULT_BUFFER_SIZE = 32;
    if (n <= 0) {
        n = DEFAULT_BUFFER_SIZE;
    }

    pb_buffer_t *buf = calloc(1, sizeof(pb_buffer_t));
    buf->cap = n;
    buf->payload = malloc(buf->cap * sizeof(uint8_t));
    return buf;
}

inline void pb_buffer_readonly(pb_buffer_t *dst, pb_buffer_t *src, size_t size) {
    dst->payload = src->payload + src->read;
    dst->cap = size;
    dst->read = 0;
    dst->write = size;
}

inline void pb_buffer_free(pb_buffer_t *buf) {
    free(buf->payload);
    free(buf);
}

inline size_t pb_buffer_size(pb_buffer_t *buf) {
    return buf->write - buf->read;
}

inline size_t pb_buffer_read(pb_buffer_t *buf, uint8_t *v, size_t n) {
    size_t read = n;
    size_t size = pb_buffer_size(buf);
    if (read > size) {
        read = size;
    }
    memcpy(v, buf->payload + buf->read, read);
    buf->read += read;
    return read;
}

inline uint8_t *pb_buffer_step_read(pb_buffer_t *buf, size_t n) {
    if (buf->read + n > buf->write) {
        return NULL;
    }
    uint8_t *payload = buf->payload + buf->read;
    buf->read += n;
    return payload;
}

size_t pb_buffer_unread(pb_buffer_t *buf, size_t n) {
    size_t unread = n;
    if (unread > buf->read) {
        unread = buf->read;
    }
    buf->read -= unread;
    return unread;
}

inline size_t pb_buffer_discard(pb_buffer_t *buf, size_t n) {
    size_t size = pb_buffer_size(buf);
    if (n > size) {
        n = size;
    }
    buf->read += n;
    return n;
}

inline pb_string_t pb_buffer_payload(pb_buffer_t *buf, size_t len) {
    size_t size = pb_buffer_size(buf);
    if (len > size) {
        len = size;
    }
    pb_string_t s = {
        .str=(const char *) buf->payload + buf->read,
        .len=len
    };
    return s;
}

void pb_buffer_grow(pb_buffer_t *buf, size_t min) {
    size_t size = pb_buffer_size(buf),
        size_free = buf->cap - size;
    if (size_free >= min) {
        return;
    }
    uint8_t *dst = buf->payload;
    if (size_free + buf->read < min) {
        buf->cap = buf->cap * 2 + min;
        dst = malloc(buf->cap * sizeof(uint8_t));
        memcpy(dst, buf->payload + buf->read, size);
    } else {
        memmove(dst, buf->payload + buf->read, size);
    }
    if (dst != buf->payload) {
        free(buf->payload);
        buf->payload = dst;
    }
    buf->read = 0;
    buf->write = size;
}

inline size_t pb_buffer_write(pb_buffer_t *buf, const uint8_t *v, size_t n) {
    pb_buffer_grow(buf, n);
    memcpy(buf->payload + buf->write, v, n);
    buf->write += n;
    return n;
}

inline uint8_t *pb_buffer_step_write(pb_buffer_t *buf, size_t n) {
    pb_buffer_grow(buf, n);
    uint8_t *payload = buf->payload + buf->write;
    buf->write += n;
    return payload;
}

size_t pb_buffer_swap_last(pb_buffer_t *buf, size_t prev_n, size_t last_n) {
    if (pb_buffer_size(buf) < prev_n + last_n) {
        return 0;
    }
    size_t s, l;
    uint8_t *tmp_ptr = buf->payload + buf->write,
        *s_dst,
        *s_ptr,
        *l_dst,
        *l_ptr;
    if (prev_n > last_n) {
        s = last_n;
        l = prev_n;
        s_ptr = tmp_ptr - s;
        l_ptr = s_ptr - l;
        s_dst = l_ptr;
        l_dst = l_ptr + s;
    } else {
        s = prev_n;
        l = last_n;
        l_ptr = tmp_ptr - l;
        s_ptr = l_ptr - s;
        l_dst = s_ptr;
        s_dst = s_ptr + l;
    }
    if (s == 0) {
        return 0;
    }
    pb_buffer_grow(buf, s);
    memcpy(tmp_ptr, s_ptr, s);
    memmove(l_dst, l_ptr, l);
    memcpy(s_dst, tmp_ptr, s);
    return l + s;
}