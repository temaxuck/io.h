# io.h

Single-header C library that provides a reusable circular buffer and a small
reader abstraction for buffered, peekable reads from POSIX file descriptors.

- Implementation enabled by defining `IO_IMPL` in **one** translation unit.
- Minimal API, small footprint, no external dependencies.

> [!WARNING]
> I've created this project for learning purposes. I **do not guarantee** that
> this code is fast, nor that it is bug-free or even well-tested. It is
> definitely **not** production-ready. The API is **not** stable also.

## Quick start

Place the header in your project. In one .c file define `IO_IMPL` before
including the header to compile the implementation:

```c
/* main.c */
#define IO_IMPL
#include "io.h"

int main(void) {
    IO_Err err;
    IO_Buffer b = {0};
    if (io_buffer_init(&b, 4096) != IO_ERR_OK) return 1;
    IO_Reader r = {0};
    io_reader_init(&r, &b, 0); // 0 == stdin

    char buf[128] = {0};
    err = io_reader_nread(&r, buf, sizeof(buf));
    /* handle err and use buf... */

    io_buffer_free(&b);
    return 0;
}
```

## Examples

### 1) Read exactly N bytes (or get IO_ERR_PARTIAL/IO_ERR_EOF)

```c
IO_Buffer b;
io_buffer_init(&b, 4096);
IO_Reader r;
io_reader_init(&r, &b, fd);

char out[256];
size_t startpos = r.pos;
IO_Err err = io_reader_nread(&r, out, sizeof(out));
if (err == IO_ERR_OK) {
    // got exactly 256 bytes
} else if (err == IO_ERR_PARTIAL) {
    // some bytes read
    // use r.pos - startpos to get how many bytes was read
} else if (err == IO_ERR_EOF) {
    // stream closed (0 bytes read)
} else {
    // other errors
}
io_buffer_free(&b);
```

### 2) Peek (non-advancing) up to N bytes

```c
// dest must be large enough, n <= buffer capacity
IO_Err err = io_reader_npeek(&r, dest, n);
if (err == IO_ERR_OK) {
    // dest contains up to n bytes; reader position unchanged
    // reader buffers read data internally
}
```

### 3) QoL API

```c
// Ensure at least N bytes are buffered (or receive partial/EOF).
IO_Err err = io_reader_prefetch(&r, n);
// handle err
size_t buffered = io_reader_buffered(&r);
for (size_t i = 0; i < buffered; i++) {
    char ch = io_buffer_at(r.b, i);
    // process ch
}
```

`io_reader_prefetch()` and `io_buffer_at()` are really just a part of *Quality
of Life API*. You don't always have to own the data that you read from the
file descriptor. Instead you may want to treat reader's internal buffer as a
source of data.

### Other examples

For other more detailed examples check
[examples/](https://github.com/temaxuck/io.h/tree/main/examples) folder.

## Design notes & semantics

- The circular buffer allocates `cap + 1` bytes; `cap` is the maximum user-visible
  capacity.
- `start == end` means *empty*. The library never uses a separate "full" flag
  thanks to the extra byte.
- `io_buffer_append()` will return `IO_ERR_OOB` if there is not enough free
  space to append `n` bytes.
- `io_buffer_nspit()` copies from the logical start (wrap-aware) without advancing.
- `io_reader_*` tracks two counters:
  - `nread`: total bytes read from the underlying fd into the internal buffer,
  - `pos`: total bytes consumed / advanced by the reader. `nread - pos` equals
    the number of bytes currently buffered (you may also use
    `io_reader_buffered()`).
- `io_reader_npeek()` may read directly into the caller's `dest` if the
  internal buffer is empty; it then appends the same bytes into the internal
  buffer to preserve consistency.

## Memory ownership

- `io_buffer_init()` allocates memory (using `IO_MALLOC`). The caller must
  call `io_buffer_free()` to free it.
- `io_reader_init()` does not allocate memory — it stores a pointer to an
  existing `IO_Buffer` (caller owns that buffer).

## Testing

The tests are located under the
[t/](https://github.com/temaxuck/io.h/tree/main/t) folder. To run the tests
use `make tests` (with optional `make clean` beforehand).

## TODOs:

This todo-list below is not in order of priority.

1. [ ] Re-write those ugly-ass tests.
2. [ ] Introduce `IO_Writer`.
3. [ ] Introduce asynchronous IO.
4. [ ] Introduce more TODOs to keep myself busy.

## License

The project is distributed under the MIT-style license included at the top of
[io.h](https://github.com/temaxuck/io.h/blob/main/io.h). Short form: free to
use, modify, distribute; provided as-is with no warranty.
