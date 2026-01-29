#include "common.h"
#include "platform.c"
#include <assert.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

enum token_type {
    T_NONE = 0u,

    T_SIMPLE = 0x8000u,

    T_LEFT_BRACKET = T_SIMPLE | 0x1u,
    T_RIGHT_BRACKET = T_SIMPLE | 0x2u,
    T_LEFT_CURLY = T_SIMPLE | 0x4u,
    T_RIGHT_CURLY = T_SIMPLE | 0x8u,
    T_COMMA = T_SIMPLE | 0x10u,
    T_COLON = T_SIMPLE | 0x20u,
    T_MINUS = T_SIMPLE | 0x40u,
    T_TRUE = T_SIMPLE | 0x80u,
    T_FALSE = T_SIMPLE | 0x100u,

    T_NUMBER_LIT,
    T_STRING_LIT,
};

struct number_lit {
    union {
        u64 integral;
        double floating;
    };
};

struct interned_string;

typedef struct token {
    enum token_type kind;
    union {
        struct {
            char simple_token;
        };
        struct number_lit num;
        struct interned_string;
    };
} token;

typedef struct lexer {
    u8 const *bytes;
    size_t len;
    size_t position;
    size_t begin_i;
    struct {
        char *key;
        byte_slice *value;
    } *string_pool;
} lexer;

static enum token_type map_char[256] = {
    [':'] = T_COLON,         [','] = T_COMMA,      ['['] = T_LEFT_BRACKET,
    [']'] = T_RIGHT_BRACKET, ['{'] = T_LEFT_CURLY, ['}'] = T_RIGHT_CURLY,
    ['-'] = T_MINUS};

// NOTE(yousef): no bounds checking
u8 consume(lexer *lexer) {
    size_t pos = lexer->position;
    lexer->position += 1;
    return lexer->bytes[pos];
}

bool push_token(token t, u8 *buffer, size_t at, size_t max_length) {
    if (at + sizeof(token) <= max_length) {
        ((token *)&buffer[at])[0] = t;
        return true;
    }
    return false;
}

typedef enum te_kind {
    TE_NONE = 0,
    TE_UNKNOWN_CHAR,
} te_kind;

typedef struct token_error {
    enum te_kind kind;
    u8 byte;
} token_error;

byte_slice *intern(lexer lexer, byte_slice *string) {
    u8 *address;
    if ((address = shget(lexer.string_pool, string)) != NULL) {
        return address;
    } else {
        shput(lexer.string_pool, string, string);
    }
}

bool match_consume_alphanumeric(lexer lexer) {}

token_error tokenize(lexer lexer, byte_slice tokens) {
    u8 c;
    size_t next_token = 0;

    while (lexer.position < lexer.len) {
        c = consume(&lexer);
        lexer.begin_i = lexer.position - 1;

        enum token_type type;

        switch (c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            // ignore whitespace
            break;

        case 't':

            break;
        case 'f':
            break;
        case '"':
            break;

        default:
            if (c >= '0' && c <= '9') {
            } else if ((type = map_char[c]) & T_SIMPLE == T_SIMPLE) {
                if (!push_token((token){type, c}, tokens.at, next_token++,
                                tokens.len)) {

                    panic("ran out of space for tokens");
                }
            } else {
                return (token_error){.kind = TE_UNKNOWN_CHAR, .byte = c};
            }
        }
    }

    return (token_error){.kind = TE_NONE};
}

int main(int argc, char const *argv[]) {
    dbg("sizeof token: %zu bytes", sizeof(token));

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
