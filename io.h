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
 * Reusable circular buffer for IO operations.
 *
 * To navigate through the buffer data, use `start` and `end` pointers.
 * `start` points to the first valid byte, `end` - to the one past the last
 * valid pointer. Consider this example:
 *
 *     buf: |a|b|c|d|e|f|g|h|
 *               ^       ^
 *               end     start
 *
 *     The effective data consists of the bytes at indexes: g, h, a, and b.
 *
 * The actual buffer size is `cap` + 1. This is needed to maintain the meaning
 * of `start` and `end` pointers, allowing these pointers to represent buffer
 * in this range of memory: [start, end).
 *
 * NOTE: Case, when `start == end`, implies an empty buffer.
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
 * Resets IO buffer `b`.
 */
IO_Err io_buffer_reset(IO_Buffer *b);

/**
 * Advances IO buffer `b` by up to `n` bytes forward and returns number of
 * bytes the start pointer was advanced.
 *
 * If `n` exceeds the number of bytes currently stored (as returned by
 * io_buffer_len()), only the available bytes are skipped.
 */
size_t io_buffer_nadvance(IO_Buffer *b, size_t n);

/**
 * Copies n bytes of data from provided buffer into a contiguous array.
 *
 * The function presumes that `dest` is a properly allocated array and has
 * enough size to store the whole buffer data.
 */
IO_Err io_buffer_nspit(IO_Buffer *src, char *dest, size_t n);

/**
 * Appends data of size n into buffer.
 *
 * The function presumes that `src` is a properly allocated contiguous array.
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
#  define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif // MIN

IO_Err io_buffer_init(IO_Buffer *b, size_t cap) {
    b->cap = cap;
    b->end = b->start = b->buf = IO_MALLOC(cap + 1);
    if (b->buf == NULL) return IO_ERR_OOM;
    return IO_ERR_OK;
}

IO_Err io_buffer_free(IO_Buffer *b) {
    free(b->buf);
    return IO_ERR_OK;
}

/**
 * Returns physical size of the buffer.
 */
static inline size_t _io_buffer_size(IO_Buffer *b) {
    return b->cap + 1;
}

size_t io_buffer_len(IO_Buffer *b) {
    if (b->end >= b->start) return b->end - b->start;
    return _io_buffer_size(b) - (b->start - b->buf) + (b->end - b->buf);
}

IO_Err io_buffer_reset(IO_Buffer *b) {
    b->start = b->end;
    return IO_ERR_OK;
}

/**
 * Returns the number of bytes between the later (farthest-from-`buf`) of
 * `b->start` and `b->end` and the physical end of the storage region.
 *
 * This is the length of the contiguous region available (or occupied) from
 * that pointer to the buffer's end; use it to determine how many bytes can be
 * read or written without wrapping back to `b->buf`.
 *
 * Example (8 bytes total, indices 0..7):
 *   case A: end >= start
 *     buf: |a|b|c|d|e|f|g|h|
 *             ^         ^
 *             start     end
 *     return value: distance from `end` to buffer end (1)
 *
 *   case B: end < start
 *     buf: |a|b|c|d|e|f|g|h|
 *             ^       ^
 *             end     start
 *     return value: distance from `start` to buffer end (2)
 */
static inline size_t _io_buffer_left_until_wrap(IO_Buffer *b) {
    if (b->end >= b->start) return _io_buffer_size(b) - (b->end - b->buf);
    return _io_buffer_size(b) - (b->start - b->buf);
}

size_t io_buffer_nadvance(IO_Buffer *b, size_t n) {
    size_t len = io_buffer_len(b);
    size_t to_shift = (n > len) ? len : n;

    size_t cur_pos = b->start - b->buf;
    size_t new_pos = (cur_pos + to_shift) % _io_buffer_size(b);

    b->start = b->buf + new_pos;
    return to_shift;
}

IO_Err io_buffer_nspit(IO_Buffer *src, char *dest, size_t n) {
    if (dest == NULL)           return IO_ERR_INV_PTR;
    if (n == 0)                 return IO_ERR_OK;
    if (n > io_buffer_len(src)) return IO_ERR_OOB;

    if (src->end >= src->start) {
        memcpy(dest, src->start, n);
        return IO_ERR_OK;
    }

    size_t to_copy = MIN(_io_buffer_left_until_wrap(src), n);
    memcpy(dest, src->start, to_copy);
    if (n > to_copy) {
        memcpy(dest + to_copy, src->buf, n-to_copy);
    }

    return IO_ERR_OK;
}

IO_Err io_buffer_append(IO_Buffer *dest, char *src, size_t n) {
    if (n == 0) return IO_ERR_OK;

    size_t len = io_buffer_len(dest);
    size_t space_left = dest->cap - len;
    if (n > space_left) return IO_ERR_OOB;

    if (dest->start > dest->end) {
        memcpy(dest->end, src, n);
        dest->end += n;
        return IO_ERR_OK;
    }

    size_t to_copy = MIN(_io_buffer_left_until_wrap(dest), n);
    memcpy(dest->end, src, to_copy);
    // TODO: Possible overflow.
    dest->end += to_copy;
    if (dest->end >= &dest->buf[_io_buffer_size(dest)]) dest->end = dest->buf;
    n -= to_copy;

    if (n > 0) {
        memcpy(dest->end, src + to_copy, n);
        dest->end += n;
    }

    return IO_ERR_OK;
}

#  endif // IO_IMPL_GUARD
#endif // IO_IMPL

