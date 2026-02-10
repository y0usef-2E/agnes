#if !defined(AG_COMMON_H)
#define AG_COMMON_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_LOG 1

#define KiB(n) (n * 1024u)
#define MiB(n) (KiB(n) * 1024u)
#define GiB(n) (MiB(n) * 1024u)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct byte_slice {
    u8 *at;
    size_t len;
} byte_slice;

typedef struct allocator {
    bool (*alloc)(size_t, u8 **out);
    void (*free)(u8 *);
} allocator_t;

#define MAX_FORMATTED_STRING_SIZE 512
static char formatted_string[MAX_FORMATTED_STRING_SIZE];

#define __fmt(...)                                                             \
    do {                                                                       \
        sprintf(formatted_string, __VA_ARGS__);                                \
    } while (0)

// NOTE(yousef): no bounds checking. This is intentional.
#define panic(...)                                                             \
    do {                                                                       \
        sprintf(formatted_string, __VA_ARGS__);                                \
        printf("[PANIC] %s (Line=%d).\n", formatted_string, __LINE__);         \
        exit(1);                                                               \
    } while (0)

#define todo(...)                                                              \
    do {                                                                       \
        char const preamble[17] = "[UNIMPLEMENTED] ";                          \
        sprintf(formatted_string, preamble);                                   \
        sprintf(formatted_string + 16, __VA_ARGS__);                           \
        panic(formatted_string);                                               \
    } while (0)

#define __dbg_at_line(...)                                                     \
    do {                                                                       \
        sprintf(formatted_string, __VA_ARGS__);                                \
        printf("[DEBUG] %s (line=%d).\n", formatted_string, __LINE__);         \
    } while (0)

#define __nop(...)                                                             \
    do {                                                                       \
    } while (0)

#if DEBUG_LOG
#define dbg(...) __dbg_at_line(__VA_ARGS__)
#else
#define dbg(...) __nop(__VA_ARGS__)
#endif

#endif