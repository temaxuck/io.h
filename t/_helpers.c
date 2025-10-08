#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../io.h"

#define T_STATIC_MEMORY_SZ 64 * 1 << 10
#define T_BUFFER_REPR_DATA_MAX_LEN 32
long T_ASSERTIONS_PASSED = 0, T_ASSERTIONS_FAILED = 0;

#define T_UNUSED(...)

#define T_FATAL(format, ...) do {               \
        printf("FATAL: "format, ##__VA_ARGS__); \
        abort();                                \
    } while(0)

#define T_ASSERT(expr, ...) ({                              \
            bool result = (expr);                           \
            if (result) {                                   \
                T_ASSERTIONS_PASSED += 1;                   \
            }                                               \
            else {                                          \
                passed = false;                             \
                T_ASSERTIONS_FAILED += 1;                   \
                printf("%s:%d: FAIL: %s: %s\n", __FILE__, __LINE__,  __func__, #expr);  \
            }                                               \
            result;                                         \
        })

#define T_EMPTY_BUFFER(cap) ({                                  \
            IO_Buffer b = {0};                                  \
            if (io_buffer_init(&b, (cap)) != IO_ERR_OK) {       \
                printf("ERROR: Failed to initialize buffer\n"); \
                abort();                                        \
            }                                                   \
            memset(b.buf, '\0', cap+1);                         \
            b;                                                  \
        })

#define T_DUP_BUFFER(b) ({                              \
            IO_Buffer d = T_EMPTY_BUFFER((b)->cap);     \
            memcpy(d.buf, (b)->buf, (b)->cap+1);        \
            d.start = d.buf + ((b)->start - (b)->buf);  \
            d.end = d.buf + ((b)->end - (b)->buf);      \
            d;                                          \
        })

char *t_buffer_repr(IO_Buffer *b) {
    static char repr[T_STATIC_MEMORY_SZ] = {0};
    size_t datalen = io_buffer_len(b);
    sprintf(repr, "IO_Buffer (at %p): data=\"%.*s\"; cap=%zu; len=%zu; start=%p (%.*s); end=%p (%.*s);",
            b, (datalen < T_BUFFER_REPR_DATA_MAX_LEN) ? (int)datalen : T_BUFFER_REPR_DATA_MAX_LEN, b->buf,
            b->cap, io_buffer_len(b),
            b->start, (int)(b->cap - (b->start - b->buf)), b->start,
            b->end,   (int)(b->cap - (b->end - b->buf)), b->end);
    return repr;
}

/**
 * Creates and initializes a new reader with buffer of capacity `cap` and file
 * descriptor `fd`.
 */
IO_Reader t_new_reader(size_t cap, int fd) {
    IO_Buffer *b = malloc(sizeof(IO_Buffer));
    if (io_buffer_init(b, cap) != IO_ERR_OK) T_FATAL("Failed to initialize buffer");
    IO_Reader r = {0};
    io_reader_init(&r, b, fd);
    return r;
}

/**
 * Creates a pipe filled with given data and returns fd for reading side.
 */
int t_new_pipe_with_data(const char *data, size_t n) {
    int fds[2];
    if (pipe(fds) == -1) T_FATAL("Failed to create pipe");
    write(fds[1], data, n);
    close(fds[1]);
    return fds[0];
}

char *t_reader_repr(IO_Reader *r) {
    static char repr[T_STATIC_MEMORY_SZ] = {0};
    sprintf(repr, "IO_Reader (at %p): b=%s; nread=%zu; pos=%zu; fd=%d;",
            r, t_buffer_repr(r->b), r->nread, r->pos, r->fd);
    return repr;
}

#define T_READER_WITH_DATA(cap, data, n) (t_new_reader((cap), t_new_pipe_with_data((data), (n))))
#define T_READER_FREE(r) ({io_buffer_free((r)->b); close((r)->fd);})
