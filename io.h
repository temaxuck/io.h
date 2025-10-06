/*
 * io.h - Simple input/output operations.
 *
 * Copyright (c) 2025 Artem Darizhapov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef IO_H
#  define IO_H

typedef enum {
    IO_ERR_OK,
    IO_ERR_OOM,         // Out of memory
    IO_ERR_OOB,         // Out of bounds
    IO_ERR_INV_PTR,     // Invalid pointer
    IO_ERR_FAILED_READ, // Failed to read from file descriptor
} IO_Err;

/**
 * Reusable buffer for IO operations.
 *
 * To navigate through the buffer data, use `start` and `end` pointers.
 * Consider this example:
 *
 *     buf: |a|b|c|d|e|f|g|h|
 *             ^         ^
 *             end       start
 *
 *     The effective data consists of the bytes at indexes: g, h, a, and b.
 */
typedef struct {
    char *buf, *start, *end;
    size_t cap;
} IO_Buffer;

/**
 * Initializes IO buffer.
 *
 * The callee must free the buffer later using `io_buffer_free()` function.
 */
IO_Err io_buffer_init(IO_Buffer *b, size_t cap);

/**
 * Frees buffer.
 */
IO_Err io_buffer_free(IO_Buffer *b);

/**
 * Returns length of a buffer data.
 */
size_t io_buffer_len(IO_Buffer *b);

/**
 * Copies n bytes of data from provided buffer into a continuous array.
 *
 * The function presumes that `dest` is a properly allocated array and has
 * enough size to store the whole buffer data.
 */
IO_Err io_buffer_nspit(IO_Buffer *src, char *dest, size_t n);

/**
 * Appends data of size n into buffer.
 *
 * The function presumes that `src` is a properly allocated continuous array.
 */
IO_Err io_buffer_append(IO_Buffer *dest, char *src, size_t n);

#endif // IO_H

#ifdef IO_IMPL
#  ifndef IO_IMPL_GUARD
#    define IO_IMPL_GUARD

#include <string.h>

#ifndef IO_MALLOC
#  include <stdlib.h>
#  define IO_MALLOC malloc
#endif // IO_MALLOC


#ifndef MIN
#  define MIN(a, b) ((a) < (b)) ? (a) : (b)
#endif // MIN

IO_Err io_buffer_init(IO_Buffer *b, size_t cap) {
    b->cap = cap;
    b->end = b->start = b->buf = IO_MALLOC(cap);
    if (b->buf == NULL) return IO_ERR_OOM;
    return IO_ERR_OK;
}

IO_Err io_buffer_free(IO_Buffer *b) {
    free(b->buf);
    return IO_ERR_OK;
}

size_t io_buffer_len(IO_Buffer *b) {
    if (b->end == b->start) return 0;
    if (b->end > b->start)  return b->end - b->start + 1;
    return b->cap - (b->start - b->buf) + (b->end - b->buf) + 1;
}

IO_Err io_buffer_nspit(IO_Buffer *src, char *dest, size_t n) {
    if (dest == NULL)           return IO_ERR_INV_PTR;
    if (n == 0)                 return IO_ERR_OK;
    if (n > io_buffer_len(src)) return IO_ERR_OOB;

    if (src->end > src->start) {
        memcpy(dest, src->start, n);
        return IO_ERR_OK;
    }

    size_t rest = src->cap - (src->start - src->buf);
    memcpy(dest, src->start, MIN(rest, n));
    if (n > rest) {
        memcpy(dest + rest, src->buf, n-rest);
    }

    return IO_ERR_OK;
}

IO_Err io_buffer_append(IO_Buffer *dest, char *src, size_t n) {
    if (n == 0) return IO_ERR_OK;

    size_t len = io_buffer_len(dest);
    size_t space_left = dest->cap - len;

    if (n > space_left) return IO_ERR_OOB;
    if (dest->start > dest->end) {
        memcpy(dest->end + 1, src, n);
        dest->end += n;
        return IO_ERR_OK;
    }

    char *last = &dest->buf[dest->cap-1];
    size_t rest = (last == dest->end) ? 0 : last - dest->end + 1;
    size_t to_copy = MIN(rest, n);
    memcpy(dest->end, src, to_copy);
    dest->end = dest->buf + to_copy - 1;
    n -= rest;

    if (n > 0) {
        memcpy(dest->buf, src + rest, n);
        dest->end = dest->buf + n - 1;
    }

    return IO_ERR_OK;
}

#  endif // IO_IMPL_GUARD
#endif // IO_IMPL

