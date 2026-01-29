// TODO(yousef): check for correctness. Is this what you want?
#if defined(_WIN32)
#include <Windows.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define KiB(n) (n * 1024u)
#define MiB(n) (KiB(n) * 1024u)
#define GiB(n) (MiB(n) * 1024u)

#define DEBUG_LOG 1

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

enum token_type {
    T_LEFT_BRACKET,
    T_RIGHT_BRACKET,
    T_LEFT_CURLY,
    T_RIGHT_CURLY,

    T_COMMA,
    T_COLON,
    T_TRUE,
    T_FALSE,
    T_HYPHEN,

    T_NUMBER_LIT,
    T_STRING_LIT,
};

typedef struct byte_slice {
    u8 const *at;
    size_t len;
} byte_slice;

struct number_lit {
    union {
        uint64_t integral;
        double floating;
    };
};

struct token {
    enum token_type kind;
    union {
        struct {
            char simple_token;
        };
        struct number_lit num;
        struct byte_slice string_lit;
    };
};

struct tokenizer {
    byte_slice bytes;
    size_t position;
    size_t begin_i;
};

#define MAX_FORMATTED_STRING_SIZE 512
static char formatted_string[MAX_FORMATTED_STRING_SIZE];

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

bool alloc_commit(size_t requested_size, u8 **out) {
#if defined(_WIN32)
    *out = (u8 *)VirtualAlloc(NULL, requested_size, MEM_COMMIT | MEM_RESERVE,
                              PAGE_READWRITE);
    return *out != NULL;
#else
    todo("memory allocation on non-win32 platforms");
#endif
}

// NOTE(yousef): assumes pathname is valid
bool read_file(char const *pathname, byte_slice *buffer) {
#if defined(_WIN32)
    HANDLE file_handle =
        CreateFileA(pathname, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        panic("unable to open \"%s\" for reading. Last Error: %lu", pathname,
              GetLastError());
    }
    DWORD _file_size_hi = 0;
    DWORD _file_size_lo = GetFileSize(file_handle, &_file_size_hi);
    DWORD file_size = _file_size_lo;
    if (_file_size_hi != 0 || file_size > buffer->len) {
        panic("file too big");
    }

    DWORD bytes_read = 0;
    bool result =
        ReadFile(file_handle, (void *)buffer->at, file_size, &bytes_read, NULL);

    if (file_size == bytes_read) {
        dbg("read: %lu bytes", file_size);
    } else {
        DWORD last_error = GetLastError();
        panic("unable to read file. file_size: %lu, bytes_read: "
              "%lu, last_error: %lu",
              file_size, bytes_read, last_error);
    }

    buffer->len = file_size;
#else

#endif
}

void tokenize(byte_slice input, struct token *buf, size_t buf_len) {}

int main(int argc, char const *argv[]) {
    // NOTE(yousef): arguments to main are validated somewhere else.
    char const *filename = argv[1];
    size_t file_size = (size_t)atoi(argv[2]); // in bytes

    size_t pool_size = file_size + MiB(200u);
    u8 *reserved_memory_pool;
    if (!alloc_commit(pool_size, &reserved_memory_pool)) {
        panic("unable to allocate required memory at start up");
    }

    u8 *input_buffer = reserved_memory_pool;
    size_t input_buffer_size = file_size;
    struct token *token_buffer =
        (struct token *)&reserved_memory_pool[input_buffer_size];
    size_t token_buffer_size = MiB(40u);

    byte_slice slice = (byte_slice){input_buffer, input_buffer_size};
    read_file(filename, &slice);
}
