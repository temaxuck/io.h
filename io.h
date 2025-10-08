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
    IO_ERR_EOF,         // End of file
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
 * Initializes IO buffer `b` wtih `cap` capacity.
 *
 * The callee must free the buffer later using `io_buffer_free()` function.
 */
IO_Err io_buffer_init(IO_Buffer *b, size_t cap);

/**
 * Frees IO buffer `b`.
 */
IO_Err io_buffer_free(IO_Buffer *b);

/**
 * Returns length of the data of IO buffer `b`.
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
 * Copies `n` bytes of data from IO buffer `src` into `dest`.
 *
 * This function presumes that `dest` is a properly allocated array and has
 * enough size to store the whole buffer data.
 */
IO_Err io_buffer_nspit(IO_Buffer *src, char *dest, size_t n);

/**
 * Appends `n` bytes from `src` into IO buffer `dest`.
 *
 * This function presumes that `src` is a properly allocated destination
 * array.
 */
IO_Err io_buffer_append(IO_Buffer *dest, char *src, size_t n);

/**
 * Reader entity.
 *
 * Represents a file descriptor reader with an associated IO_Buffer. Use this
 * to perform buffered read operations without manually managing partial reads
 * or file descriptor offsets.
 */
typedef struct {
    IO_Buffer *b;
    size_t nread, pos;
    int fd;
} IO_Reader;

/**
 * Initializes reader `r` with IO Buffer `b` and a file descriptor `fd`.
 */
IO_Err io_reader_init(IO_Reader *r, IO_Buffer *b, int fd);

/**
 * Returns number of buffered bytes by reader (`r`).
 *
 * Reader buffers data when performing `peek` operation. See docs for
 * io_reader_npeek().
 */
size_t io_reader_buffered(IO_Reader *r);

/**
 * Reads up to `n` bytes from reader (`r`) into destination buffer (`dest`)
 * **without** advancing the reader's position. Data is first read (if needed)
 * into the reader's internal buffer, and then copied to `dest`.
 *
 * This function ensures that the requested number of bytes (`n`) fits within
 * the reader's buffer capacity. If `n` exceeds the buffer capacity, it
 * returns `IO_ERR_OOB` ("out of bounds"). This behavior is *expected* because
 * the peek operation depends on the buffer to hold all requested data for
 * potential future reads. Allowing a peek size larger than the buffer would
 * break this guarantee and risk partial or inconsistent results. In short:
 * the buffer defines the maximum peekable window size.
 *
 * On partial reads (e.g., end-of-file reached before `n` bytes could be
 * read), returns `IO_ERR_EOF` and copies as much data as was available.
 */
IO_Err io_reader_npeek(IO_Reader *r, char *dest, size_t n);

/**
 * Reads up to `n` bytes from reader (`r`) into destination buffer (`dest`),
 * advancing the reader's position by the number of bytes successfully read.
 *
 * This function first consumes any available data from the reader's internal
 * buffer. If the buffer does not contain enough data to satisfy the request,
 * additional bytes are read directly from the underlying file descriptor and
 * appended to `dest`. Unlike `io_reader_npeek`, this operation advances the
 * reader's position and discards consumed data from the buffer.
 *
 * On partial reads (e.g., if EOF is reached before `n` bytes are read),
 * returns `IO_ERR_EOF` and copies as much data as was available.
 */
IO_Err io_reader_nread(IO_Reader *r, char *dest, size_t n);

/**
 * Consumes up to `n` bytes from reader (`r`)'s internal buffer, copying
 * consumed data into `dest` (if non-NULL) and advancing the reader's position
 * accordingly.
 *
 * If fewer than `n` bytes are available in the buffer, all available bytes
 * are consumed. The function does not read additional data from the file
 * descriptor.
 *
 * If `dest` is NULL, the function simply advances the buffer position without
 * copying any data. Passing `n == 0` and a non-NULL `dest` is valid and
 * treated as a no-op.
 */
IO_Err io_reader_nconsume(IO_Reader *r, char *dest, size_t n);

/**
 * Discards all data currently stored in reader (`r`)'s internal buffer,
 * resetting it to an empty state and advancing the reader's position by the
 * number of discarded bytes.
 *
 * This operation does not affect the underlying file descriptor — it simply
 * clears any buffered data that has already been read into memory.
 */
IO_Err io_reader_discard(IO_Reader *r);

#endif // IO_H

#ifdef IO_IMPL
#  ifndef IO_IMPL_GUARD
#    define IO_IMPL_GUARD

#include <string.h>

#ifndef IO_ASSERT
#  ifdef _DEBUG
#    include <assert.h>
#    define IO_ASSERT(expr) do { assert(expr); } while(0)
#  else
#    define IO_ASSERT(expr)
#  endif // DEBUG
#endif // IO_ASSERT

#ifndef IO_MALLOC
#  include <stdlib.h>
#  define IO_MALLOC malloc
#endif // IO_MALLOC

#ifndef IO_READ
// TODO: Depending on platform, use different implementations of `read()`
#  include <unistd.h>
#  define IO_READ read
#endif // IO_READ


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
    IO_ASSERT(b->start <= b->buf + b->cap && "Out of bounds");

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
    dest->end += to_copy;
    if (dest->end == dest->buf + _io_buffer_size(dest)) dest->end = dest->buf;
    n -= to_copy;

    if (n > 0) {
        memcpy(dest->end, src + to_copy, n);
        dest->end += n;
    }

    IO_ASSERT(dest->end <= dest->buf + _io_buffer_size(dest) && "Out of bounds");

    return IO_ERR_OK;
}

IO_Err io_reader_init(IO_Reader *r, IO_Buffer *b, int fd) {
    r->b = b;
    r->fd = fd;
    r->pos = r->nread = 0;
    return IO_ERR_OK;
}

size_t io_reader_buffered(IO_Reader *r) {
    IO_ASSERT(r->nread >= r->pos && r->nread - r->pos == io_buffer_len(r->b) && "Out of bounds");
    return r->nread - r->pos;
}

IO_Err io_reader_npeek(IO_Reader *r, char *dest, size_t n) {
    if (n == 0) return IO_ERR_OK;
    if (n > r->b->cap) return IO_ERR_OOB;

    IO_Err result = IO_ERR_OK, err = result;
    size_t buflen = io_buffer_len(r->b);
    size_t to_read = n - MIN(n, buflen);

    if (to_read > 0) {
        int nread = IO_READ(r->fd, dest + buflen, to_read);
        if (nread < 0) return IO_ERR_FAILED_READ;
        if ((err = io_buffer_append(r->b, dest + buflen, nread)) && err != IO_ERR_OK) return err;
        r->nread += nread;
        buflen = io_buffer_len(r->b);
        if ((size_t)nread < to_read) result = IO_ERR_EOF;
    }

    // NOTE: Reader may not store exactly n bytes, in case when fd provided
    //       fewer bytes than requested.
    if ((err = io_buffer_nspit(r->b, dest, MIN(io_reader_buffered(r), n))) && err != IO_ERR_OK) return err;
    return result;
}

IO_Err io_reader_nconsume(IO_Reader *r, char *dest, size_t n) {
    if (n == 0 && dest != NULL) return IO_ERR_OK;
    IO_Err err;

    size_t to_copy = MIN(n, io_buffer_len(r->b));
    if (dest != NULL) {
        if ((err = io_buffer_nspit(r->b, dest, to_copy)) && err != IO_ERR_OK) return err;
    }

    io_buffer_nadvance(r->b, to_copy);
    r->pos += to_copy;

    IO_ASSERT(r->pos <= r->nread && "Out of bounds");
    return IO_ERR_OK;
}

IO_Err io_reader_discard(IO_Reader *r) {
    r->pos += io_buffer_len(r->b);
    io_buffer_reset(r->b);

    IO_ASSERT(r->pos == r->nread && "Out of bounds");
    return IO_ERR_OK;
}

IO_Err io_reader_nread(IO_Reader *r, char *dest, size_t n) {
    if (n == 0) return IO_ERR_OK;
    IO_Err result = IO_ERR_OK, err = result;

    size_t start_pos = r->pos;
    if ((err = io_reader_nconsume(r, dest, n)) && err != IO_ERR_OK) return err;

    size_t copied = r->pos - start_pos;
    size_t to_read = n - copied;
    if (to_read > 0) {
        int nread = IO_READ(r->fd, dest + copied, to_read);
        if (nread < 0) return IO_ERR_FAILED_READ;
        r->pos += nread;
        r->nread += nread;
        if ((size_t)nread < to_read) result = IO_ERR_EOF;
    }

    IO_ASSERT(r->pos <= r->nread && "Out of bounds");
    return result;
}

#  endif // IO_IMPL_GUARD
#endif // IO_IMPL
