long T_PASSED = 0;
long T_FAILED = 0;

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

#define T_ASSERT(expr, ...) ({                              \
            bool result = (expr);                           \
            if (result) {                                   \
                printf("PASS: %s: %s\n", __func__, #expr);  \
                T_PASSED += 1;                              \
            }                                               \
            else {                                          \
                T_FAILED += 1;                              \
                printf("FAIL: %s: %s\n", __func__, #expr);  \
            }                                               \
            result;                                         \
        })
