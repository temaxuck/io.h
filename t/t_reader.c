/*
 * TODO: Come up with some small testing framework.
 * TODO: Re-write tests, because right now it's just a huge slop.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define IO_IMPL
#define _DEBUG
#include "../io.h"
#include "_helpers.c"

static long T_READER_PASSED = 0, T_READER_FAILED = 0;

#define T_READER_ASSERT_FOR_READER(expr, r) do {                \
        bool result = T_ASSERT(expr);                           \
        if (!result) printf("INFO: %s\n", t_reader_repr(r));    \
        result;                                                 \
    } while(0)

#define T_READER_ASSERT_BUFFER_SW(r, exp, len) do {                     \
        size_t buflen = io_buffer_len((r)->b);                          \
        if (buflen < (len)) {T_ASSERT(false); break; }                  \
        char actual[(len)] = {0};                                       \
        IO_Err _err;                                                    \
        if ((_err = io_buffer_nspit((r)->b, actual, (len)) && _err != IO_ERR_OK)) { \
            printf("ERROR: io_buffer_nspit() failed: %d", _err);        \
            T_ASSERT(false);                                            \
            break;                                                      \
        }                                                               \
        T_ASSERT(strncmp(actual, (exp), (len)) == 0);                   \
    } while (0)

#define T_READER_ASSERT_BUFFER_EQ(r, exp, len) do {                     \
        size_t buflen = io_buffer_len((r)->b);                          \
        if (buflen != (len)) { T_ASSERT(false); break; }                \
        if ((len) == 0) { T_ASSERT(true); break; }                      \
        char actual[((len) <= 0) ? 1 : (len)] = {0};                    \
        IO_Err _err = IO_ERR_OK;                                        \
        if ((_err = io_buffer_nspit((r)->b, actual, (len)) && _err != IO_ERR_OK)) { \
            printf("ERROR: io_buffer_nspit() failed: %d", _err);        \
            T_ASSERT(false);                                            \
            break;                                                      \
        }                                                               \
            T_ASSERT(strncmp(actual, (exp), (len)) == 0);               \
    } while (0)



//////////////////// BEGIN: TEST CASES IMPLEMENTATION ////////////////////
bool t_reader_case_init_empty(void) {
    bool passed = true;
    IO_Buffer b = T_EMPTY_BUFFER(8);
    IO_Reader r = {0};

    T_ASSERT(io_reader_init(&r, &b, 0) == IO_ERR_OK);
    T_ASSERT(r.b->cap == 8);
    T_ASSERT(r.pos == 0);
    T_ASSERT(r.nread == 0);

    T_READER_FREE(&r);
    return passed;
}

bool t_reader_case_peek_less_than_buffer_capacity(void) {
    bool passed = true;
    IO_Reader r = T_READER_WITH_DATA(8, "ABCDEFG", 7);

    char dest[8] = {0};
    T_ASSERT(io_reader_npeek(&r, dest, 4) == IO_ERR_OK);
    if (!T_ASSERT(strcmp(dest, "ABCD") == 0)) printf("ERROR: dest=%.*s (expected: ABCD)\n", 4, dest);
    T_READER_ASSERT_BUFFER_EQ(&r, "ABCD", 4);
    T_ASSERT(r.pos == 0);
    T_READER_ASSERT_FOR_READER(r.nread == 4, &r);

    T_READER_FREE(&r);
    return passed;
}

bool t_reader_case_peek_more_than_available_data(void) {
    bool passed = true;
    IO_Reader r = T_READER_WITH_DATA(8, "XYZ", 3);

    char dest[8] = {0};
    T_ASSERT(io_reader_npeek(&r, dest, 6) == IO_ERR_EOF);
    if (!T_ASSERT(strcmp(dest, "XYZ") == 0)) printf("ERROR: dest=%.*s (expected: XYZ)\n", 3, dest);
    T_READER_ASSERT_BUFFER_EQ(&r, "XYZ", 3);
    T_ASSERT(r.pos == 0);
    T_READER_ASSERT_FOR_READER(r.nread == 3, &r);

    T_READER_FREE(&r);
    return passed;
}

bool t_reader_case_peek_larger_than_buffer_capacity(void) {
    bool passed = true;
    IO_Reader r = T_READER_WITH_DATA(4, "ABCDEFGHIJ", 10);

    char dest[8] = {0};
    T_ASSERT(io_reader_npeek(&r, dest, 10) == IO_ERR_OOB);
    T_ASSERT(strlen(dest) == 0);
    T_ASSERT(r.pos == 0);
    T_READER_ASSERT_FOR_READER(r.nread == 0, &r);

    T_READER_FREE(&r);

    return passed;
}

bool t_reader_case_consume_from_prefilled_buffer(void) {
    bool passed = true;

    IO_Buffer b = T_EMPTY_BUFFER(5);
    if (io_buffer_append(&b, "HELLO", 5) != IO_ERR_OK) T_FATAL("Failed to pre-fill reader's buffer");
    IO_Reader r = {.b=&b, .pos=0, .nread=5, .fd=0};

    char dest[8] = {0};
    T_ASSERT(io_reader_nconsume(&r, dest, 3) == IO_ERR_OK);
    if (!T_ASSERT(strcmp(dest, "HEL") == 0)) printf("ERROR: dest=%.*s (expected: HEL)\n", 3, dest);
    T_READER_ASSERT_FOR_READER(r.pos == 3, &r);
    T_READER_ASSERT_BUFFER_EQ(&r, "LO", 2);

    T_READER_FREE(&r);

    return passed;
}

bool t_reader_case_consume_without_dest(void) {
    bool passed = true;

    IO_Buffer b = T_EMPTY_BUFFER(5);
    if (io_buffer_append(&b, "HELLO", 5) != IO_ERR_OK) T_FATAL("Failed to pre-fill reader's buffer");
    IO_Reader r = {.b=&b, .pos=0, .nread=5, .fd=0};

    T_ASSERT(io_reader_nconsume(&r, NULL, 3) == IO_ERR_OK);
    T_READER_ASSERT_FOR_READER(r.pos == 3, &r);
    T_ASSERT(io_buffer_len(r.b) == 2);

    T_READER_FREE(&r);

    return passed;
}

bool t_reader_case_discard(void) {
    bool passed = true;

    IO_Buffer b = T_EMPTY_BUFFER(5);
    if (io_buffer_append(&b, "WORLD", 5) != IO_ERR_OK) T_FATAL("Failed to pre-fill reader's buffer");
    IO_Reader r = {.b=&b, .pos=0, .nread=5, .fd=0};

    T_ASSERT(io_reader_discard(&r) == IO_ERR_OK);
    T_READER_ASSERT_FOR_READER(r.pos == r.nread && r.pos == 5, &r);
    T_ASSERT(io_buffer_len(r.b) == 0);

    T_READER_FREE(&r);
    return passed;
}

bool t_reader_case_normal_read_partial_from_buffer_plus_fd(void) {
    bool passed = true;
    IO_Reader r = T_READER_WITH_DATA(5, "AB123", 5);

    char dest[8] = {0};
    T_ASSERT(io_reader_npeek(&r, dest, 2) == IO_ERR_OK);
    T_READER_ASSERT_BUFFER_EQ(&r, "AB", 2);
    T_READER_ASSERT_FOR_READER(r.nread == 2, &r);
    memset(dest, 0, 8);

    T_ASSERT(io_reader_nread(&r, dest, 5) == IO_ERR_OK);
    if (!T_ASSERT(strcmp(dest, "AB123") == 0)) printf("ERROR: dest=%.*s (expected: AB123)\n", 5, dest);
    T_READER_ASSERT_BUFFER_EQ(&r, "", 0);
    T_READER_ASSERT_FOR_READER(r.pos == 5 && r.nread == 5, &r);

    T_READER_FREE(&r);
    return passed;
}

bool t_reader_case_read_hitting_EOF(void) {
    bool passed = true;
    IO_Reader r = T_READER_WITH_DATA(5, "AB", 2);

    char dest[8] = {0};
    T_ASSERT(io_reader_nread(&r, dest, 5) == IO_ERR_EOF);
    if (!T_ASSERT(strcmp(dest, "AB") == 0)) printf("ERROR: dest=%.*s (expected: AB)\n", 2, dest);
    T_READER_ASSERT_BUFFER_EQ(&r, "", 0);
    T_READER_ASSERT_FOR_READER(r.pos == 2 && r.nread == 2, &r);

    T_READER_FREE(&r);
    return passed;
}

bool t_reader_case_sequence_peek_consume_read_peek_discard(void) {
    bool passed = true;
    IO_Reader r = T_READER_WITH_DATA(8, "ABCDEFGH", 8);

    char dest[8] = {0};
    T_ASSERT(io_reader_npeek(&r, dest, 3) == IO_ERR_OK);
    T_READER_ASSERT_BUFFER_EQ(&r, "ABC", 3);
    T_ASSERT(strcmp(dest, "ABC") == 0);
    T_READER_ASSERT_FOR_READER(r.pos == 0 && r.nread == 3, &r);

    memset(dest, 0, 8);
    T_ASSERT(io_reader_nconsume(&r, dest, 2) == IO_ERR_OK);
    T_READER_ASSERT_BUFFER_EQ(&r, "C", 1);
    T_ASSERT(strcmp(dest, "AB") == 0);
    T_READER_ASSERT_FOR_READER(r.pos == 2 && r.nread == 3, &r);

    memset(dest, 0, 8);
    T_ASSERT(io_reader_nread(&r, dest, 3) == IO_ERR_OK);
    T_READER_ASSERT_BUFFER_EQ(&r, "", 0);
    T_ASSERT(strcmp(dest, "CDE") == 0);
    T_READER_ASSERT_FOR_READER(r.pos == 5 && r.nread == 5, &r);

    memset(dest, 0, 8);
    T_ASSERT(io_reader_npeek(&r, dest, 8) == IO_ERR_EOF);
    T_READER_ASSERT_BUFFER_EQ(&r, "FGH", 3);
    T_ASSERT(strcmp(dest, "FGH") == 0);
    T_READER_ASSERT_FOR_READER(r.pos == 5 && r.nread == 8, &r);

    T_ASSERT(io_reader_discard(&r) == IO_ERR_OK);
    T_READER_ASSERT_BUFFER_EQ(&r, "", 0);
    T_READER_ASSERT_FOR_READER(r.pos == 8 && r.nread == 8, &r);

    T_READER_FREE(&r);
    return passed;
}

//////////////////// END:   TEST CASES IMPLEMENTATION ////////////////////
// TODO: Test wrapping peek/read
#define T_READER_CASE_MAP(XX)                                       \
    XX(1,  init_empty)                                              \
    XX(2,  peek_less_than_buffer_capacity)                          \
    XX(3,  peek_more_than_available_data)                           \
    XX(4,  peek_larger_than_buffer_capacity)                        \
    XX(5,  consume_from_prefilled_buffer)                           \
    XX(6,  consume_without_dest)                                    \
    XX(7,  discard)                                                 \
    XX(8,  normal_read_partial_from_buffer_plus_fd)                 \
    XX(9,  read_hitting_EOF)                                        \
    XX(10, sequence_peek_consume_read_peek_discard)


void t_buffer_run(void) {
#define XX(num, name) do {                                              \
        printf("\tRunning  testcase \"%s\"\n", #name);               \
        bool result = t_reader_case_##name();                           \
        printf("\tFinished testcase \"%s\": %s\n", #name, (result) ? "PASSED" : "FAILED"); \
        if (result) T_READER_PASSED++; else T_READER_FAILED++;          \
    } while(0);

    T_READER_CASE_MAP(XX);
#undef XX
    printf("INFO: Passed: %ld/%ld; Assertions %ld/%ld\n", T_READER_PASSED, T_READER_PASSED + T_READER_FAILED, T_ASSERTIONS_PASSED, T_ASSERTIONS_PASSED + T_ASSERTIONS_FAILED);
}

int main() {
    t_buffer_run();
    return 0;
}
