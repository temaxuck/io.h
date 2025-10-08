#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define IO_IMPL
#define _DEBUG
#include "../io.h"
#include "_helpers.c"

#define T_BUFFER_STATIC_MEMORY_SZ 64 * 1 << 10
#define T_BUFFER_REPR_DATA_MAX_LEN 32

#define T_ASSERT_FOR_BUFFER(expr, b) ({                             \
            bool result = T_ASSERT(expr);                           \
            if (!result) printf("INFO: %s\n", t_buffer_repr(b));    \
            result;                                                 \
        })

char *t_buffer_repr(IO_Buffer *b) {
    static char repr[T_BUFFER_STATIC_MEMORY_SZ] = {0};
    size_t datalen = io_buffer_len(b);
    sprintf(repr, "IO_Buffer (at %p): data=\"%.*s\"; cap=%zu; len=%zu; start=%p (%.*s); end=%p (%.*s);",
            b, (datalen < T_BUFFER_REPR_DATA_MAX_LEN) ? (int)datalen : T_BUFFER_REPR_DATA_MAX_LEN, b->buf,
            b->cap, io_buffer_len(b),
            b->start, (int)(b->cap - (b->start - b->buf)), b->start,
            b->end,   (int)(b->cap - (b->end - b->buf)), b->end);
    return repr;
}

bool t_buffer_cmp(IO_Buffer *a, IO_Buffer *b) {
    if (a->cap != b->cap) return false;

    for (size_t i = 0; i < a->cap; i++) if (a->buf[i] != b->buf[i]) return false;

    if (a->start - a->buf != b->start - b->buf) return false;
    if (a->end - a->buf != b->end - b->buf) return false;

    return true;
}

//////////////////// BEGIN: TEST CASES IMPLEMENTATION ////////////////////
void t_buffer_case_init_empty(void) {
    IO_Buffer before = {0};
    T_ASSERT(io_buffer_init(&before, 6) == IO_ERR_OK);
    T_ASSERT_FOR_BUFFER(before.end == before.start && before.start == before.buf, &before);

    IO_Buffer after  = T_DUP_BUFFER(&before);
    size_t len = io_buffer_len(&after);
    T_ASSERT_FOR_BUFFER(len == 0, &after);
    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_append_exactly_capacity_no_wrap(void) {
    IO_Buffer b = T_EMPTY_BUFFER(6);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&b, "123456", 6) == IO_ERR_OK, &b);
    size_t len = io_buffer_len(&b);
    T_ASSERT_FOR_BUFFER(len == 6, &b);
    T_ASSERT_FOR_BUFFER(strncmp(b.buf, "123456", len) == 0, &b);

    io_buffer_free(&b);
}

void t_buffer_case_append_beyond_capacity(void) {
    IO_Buffer before = T_EMPTY_BUFFER(6);
    IO_Buffer after  = T_DUP_BUFFER(&before);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&after, "1234567", 7) == IO_ERR_OOB, &after);
    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_append_zero_bytes(void) {
    IO_Buffer before = T_EMPTY_BUFFER(6);
    IO_Buffer after  = T_DUP_BUFFER(&before);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&after, "", 0) == IO_ERR_OK, &after);
    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_nspit_contiguous_read_no_wrap(void) {
    IO_Buffer before = T_EMPTY_BUFFER(6);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&before, "123456", 6) == IO_ERR_OK, &before);

    IO_Buffer after = T_DUP_BUFFER(&before);
    char dest[6] = {0};
    T_ASSERT(io_buffer_nspit(&after, dest, 6) == IO_ERR_OK);
    T_ASSERT(strncmp(dest, "123456", 6) == 0);

    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_nspit_partial_contiguous_read(void) {
    IO_Buffer before = T_EMPTY_BUFFER(6);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&before, "123456", 6) == IO_ERR_OK, &before);

    IO_Buffer after = T_DUP_BUFFER(&before);
    char dest[3] = {0};
    T_ASSERT(io_buffer_nspit(&after, dest, 3) == IO_ERR_OK);
    T_ASSERT(strncmp(dest, "123", 3) == 0);

    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_wrapped_buffer_read_nspit_across_wrap(void) {
    char *raw = malloc(7);
    raw[0] = 'D';
    raw[1] = '\0';
    raw[2] = '\0';
    raw[3] = '\0';
    raw[4] = 'A';
    raw[5] = 'B';
    raw[6] = 'C';
    IO_Buffer before = {.cap=6, .buf=raw, .start=raw+4, .end=raw+1};
    size_t len = io_buffer_len(&before);
    T_ASSERT_FOR_BUFFER(len == 4, &before);

    IO_Buffer after = T_DUP_BUFFER(&before);
    char dest[4] = {0};
    T_ASSERT(io_buffer_nspit(&after, dest, 4) == IO_ERR_OK);
    if (!T_ASSERT(strncmp(dest, "ABCD", 4) == 0)) printf("ERROR: dest=%.*s (expected: ABCD)\n", 4, dest);
    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_append_when_buffer_is_wrapped_fills_remaining_capacity(void) {
    char *raw = malloc(7);
    raw[0] = 'D';
    raw[1] = '\0';
    raw[2] = '\0';
    raw[3] = '\0';
    raw[4] = 'A';
    raw[5] = 'B';
    raw[6] = 'C';
    IO_Buffer before = {.cap=6, .buf=raw, .start=raw+4, .end=raw+1};
    IO_Buffer after  = T_DUP_BUFFER(&before);

    T_ASSERT(io_buffer_append(&after, "EF", 2) == IO_ERR_OK);
    T_ASSERT(io_buffer_len(&after) == 6);
    T_ASSERT(after.start - after.buf == before.start - before.buf);
    T_ASSERT(after.end == after.buf + 3);

    char dest[6] = {0};
    T_ASSERT(io_buffer_nspit(&after, dest, 6) == IO_ERR_OK);
    if (!T_ASSERT(strncmp(dest, "ABCDEF", 6) == 0)) printf("ERROR: dest=%.*s (expected: ABCDEF)\n", 4, dest);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_append_that_would_exceed_remaining_capacity(void) {
    char *raw = malloc(7);
    raw[0] = 'D';
    raw[1] = '\0';
    raw[2] = '\0';
    raw[3] = '\0';
    raw[4] = 'A';
    raw[5] = 'B';
    raw[6] = 'C';
    IO_Buffer before = {.cap=6, .buf=raw, .start=raw+4, .end=raw+1};
    IO_Buffer after  = T_DUP_BUFFER(&before);

    T_ASSERT(io_buffer_append(&after, "EFG", 3) == IO_ERR_OOB);
    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_nspit_requesting_more_bytes_than_available(void) {
    IO_Buffer b = T_EMPTY_BUFFER(6);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&b, "123456", 6) == IO_ERR_OK, &b);

    char dest[7] = {0};
    T_ASSERT(io_buffer_nspit(&b, dest, 7) == IO_ERR_OOB);
    if (!T_ASSERT(strcmp(dest, "") == 0)) printf("ERROR: dest=%.*s (expected: (null))\n", 4, dest);

    io_buffer_free(&b);
}

void t_buffer_case_nspit_reading_across_the_wrap_boundary_partial(void) {
    char *raw = malloc(7);
    raw[0] = 'C';
    raw[1] = 'D';
    raw[2] = '\0';
    raw[3] = '\0';
    raw[4] = '\0';
    raw[5] = 'A';
    raw[6] = 'B';
    IO_Buffer before = {.cap=6, .buf=raw, .start=raw+5, .end=raw+2};
    IO_Buffer after  = T_DUP_BUFFER(&before);

    char dest[3] = {0};
    T_ASSERT(io_buffer_nspit(&after, dest, 3) == IO_ERR_OK);
    if (!T_ASSERT(strncmp(dest, "ABC", 3) == 0)) printf("ERROR: dest=%.*s (expected: ABC)\n", 3, dest);

    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_minimal_capacity_cap_eq_1(void) {
    IO_Buffer before = {0};
    T_ASSERT(io_buffer_init(&before, 1) == IO_ERR_OK);
    T_ASSERT_FOR_BUFFER(before.end == before.start && before.start == before.buf, &before);

    T_ASSERT(io_buffer_append(&before, "A", 1) == IO_ERR_OK);
    T_ASSERT(io_buffer_len(&before) == 1);
    T_ASSERT_FOR_BUFFER(before.buf[0] == 'A', &before);
    T_ASSERT_FOR_BUFFER(before.start == before.buf, &before);
    T_ASSERT_FOR_BUFFER(before.end == before.buf + 1, &before);

    char dest[1] = {0};
    T_ASSERT(io_buffer_nspit(&before, dest, 1) == IO_ERR_OK);
    if (!T_ASSERT(strncmp(dest, "A", 1) == 0)) printf("ERROR: dest=%.*s (expected: ABC)\n", 1, dest);

    IO_Buffer after  = T_DUP_BUFFER(&before);
    T_ASSERT(io_buffer_append(&after, "B", 1) == IO_ERR_OOB);
    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_nspit_with_n_eq_0(void) {
    IO_Buffer before = T_EMPTY_BUFFER(6);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&before, "123456", 6) == IO_ERR_OK, &before);

    IO_Buffer after = T_DUP_BUFFER(&before);
    const char *orig = "The string must not change";
    char *dest = strdup(orig);
    T_ASSERT(io_buffer_nspit(&after, dest, 0) == IO_ERR_OK);
    T_ASSERT(strcmp(dest, orig) == 0);
    T_ASSERT(t_buffer_cmp(&after, &before) == true);

    free(dest);
    io_buffer_free(&before);
    io_buffer_free(&after);
}

void t_buffer_case_nadvance_contiguous_data(void) {
    IO_Buffer b = T_EMPTY_BUFFER(6);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&b, "123456", 6) == IO_ERR_OK, &b);

    T_ASSERT(io_buffer_nadvance(&b, 4) == 4);
    T_ASSERT_FOR_BUFFER(b.start == b.buf + 4, &b);
    T_ASSERT_FOR_BUFFER(b.end == b.buf + 6, &b);

    io_buffer_free(&b);
}

void t_buffer_case_nadvance_wrap(void) {
    char *raw = malloc(7);
    raw[0] = 'C';
    raw[1] = 'D';
    raw[2] = '\0';
    raw[3] = '\0';
    raw[4] = '\0';
    raw[5] = 'A';
    raw[6] = 'B';
    IO_Buffer b = {.cap=6, .buf=raw, .start=raw+5, .end=raw+2};

    T_ASSERT(io_buffer_nadvance(&b, 3) == 3);
    T_ASSERT_FOR_BUFFER(b.start == b.buf + 1, &b);
    T_ASSERT_FOR_BUFFER(b.end == b.buf + 2, &b);

    io_buffer_free(&b);
}

void t_buffer_case_nadvance_to_0(void) {
    IO_Buffer b = T_EMPTY_BUFFER(6);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&b, "123456", 6) == IO_ERR_OK, &b);

    T_ASSERT(io_buffer_nadvance(&b, 0) == 0);
    T_ASSERT_FOR_BUFFER(b.start == b.buf, &b);
    T_ASSERT_FOR_BUFFER(b.end == b.buf + 6, &b);

    io_buffer_free(&b);
}

void t_buffer_case_nadvance_with_empty_buffer(void) {
    IO_Buffer b = T_EMPTY_BUFFER(6);

    T_ASSERT(io_buffer_nadvance(&b, 4) == 0);
    T_ASSERT_FOR_BUFFER(b.start == b.buf && b.end == b.buf, &b);

    io_buffer_free(&b);
}

void t_buffer_case_nadvance_beyond_capacity(void) {
    IO_Buffer b = T_EMPTY_BUFFER(6);
    T_ASSERT_FOR_BUFFER(io_buffer_append(&b, "123456", 6) == IO_ERR_OK, &b);

    T_ASSERT(io_buffer_nadvance(&b, 69) == 6);
    T_ASSERT_FOR_BUFFER(b.start == b.end && b.end == b.buf + 6, &b);

    io_buffer_free(&b);
}
//////////////////// END:   TEST CASES IMPLEMENTATION ////////////////////

#define T_BUFFER_CASE_MAP(XX)                                       \
    XX(1,  init_empty)                                              \
    XX(2,  append_exactly_capacity_no_wrap)                         \
    XX(3,  append_beyond_capacity)                                  \
    XX(4,  append_zero_bytes)                                       \
    XX(5,  nspit_contiguous_read_no_wrap)                           \
    XX(6,  nspit_partial_contiguous_read)                           \
    XX(7,  wrapped_buffer_read_nspit_across_wrap)                   \
    XX(8,  append_when_buffer_is_wrapped_fills_remaining_capacity)  \
    XX(9,  append_that_would_exceed_remaining_capacity)             \
    XX(10, nspit_requesting_more_bytes_than_available)              \
    XX(11, nspit_reading_across_the_wrap_boundary_partial)          \
    XX(12, minimal_capacity_cap_eq_1)                               \
    XX(13, nspit_with_n_eq_0)                                       \
    XX(14, nadvance_contiguous_data)                                \
    XX(15, nadvance_wrap)                                           \
    XX(16, nadvance_to_0)                                           \
    XX(17, nadvance_with_empty_buffer)                              \
    XX(18, nadvance_beyond_capacity)

void t_buffer_run(void) {
#define XX(num, name) do {                                              \
        printf("#################### BEGIN: TC \"%s\" ####################\n", #name); \
        t_buffer_case_##name();                                         \
        printf("#################### END:   TC \"%s\" ####################\n", #name); \
    } while(0);

      T_BUFFER_CASE_MAP(XX);
#undef XX

      printf("INFO: PASSED: %ld; FAILED: %ld.\n", T_PASSED, T_FAILED);
}

int main(void) {
    t_buffer_run();
    return 0;
}
