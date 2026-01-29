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

    T_NUMBER_LIT = 1,
    T_STRING_LIT = 2,
    T_IDENTIFIER = 3,
};

struct number_lit {
    union {
        u64 integral;
        double floating;
    };
};

typedef struct token {
    enum token_type kind;
    union {
        struct {
            char simple_token;
        };
        struct number_lit num;
        byte_slice string_lit;
        byte_slice identifier;
    };
} token_t;

typedef struct lexer {
    u8 const *bytes;
    size_t len;
    size_t position;
    size_t begin_i;
    struct {
        char *key;
        byte_slice value;
    } *string_table;
    u8 *string_stack;
    size_t next_string;
    size_t stack_size;
} lexer_t;

static enum token_type map_char[256] = {
    [':'] = T_COLON,         [','] = T_COMMA,      ['['] = T_LEFT_BRACKET,
    [']'] = T_RIGHT_BRACKET, ['{'] = T_LEFT_CURLY, ['}'] = T_RIGHT_CURLY,
    ['-'] = T_MINUS};

// NOTE(yousef): no bounds checking
u8 consume(lexer_t *lexer) {
    size_t pos = lexer->position;
    lexer->position += 1;
    return lexer->bytes[pos];
}

bool push_token(token_t t, u8 *buffer, size_t at, size_t max_length) {
    if (at + sizeof(token_t) <= max_length) {
        ((token_t *)&buffer[at])[0] = t;
        return true;
    }
    return false;
}

typedef enum te_kind {
    TE_NONE = 0,
    TE_UNKNOWN_TOKEN,
} te_kind;

typedef struct token_error {
    enum te_kind kind;
    struct token token;
} token_error;

byte_slice intern(lexer_t *lexer, byte_slice string) {
    byte_slice slice;
    if ((slice = shget(lexer->string_table, string.at)).at != NULL) {
        return slice;
    } else {
        return shput(lexer->string_table, string.at, string);
    }
}

bool match_consume_alphanumeric(lexer_t *lexer) {
    size_t pos = lexer->position;
    u8 c = lexer->bytes[pos];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        c >= '0' && c <= '9') {
        lexer->position += 1;
        return true;
    }
    return false;
}

u8 temp_cstrings[MiB(1)];
size_t next_temp_cstring = 0;
byte_slice temp_cstring(char const *source) {
    size_t src_len = strlen(source);

    if (next_temp_cstring + src_len >
        sizeof(temp_cstrings) / sizeof(temp_cstrings[0])) {
        next_temp_cstring = 0;
    }

    u8 *base = &temp_cstrings[next_temp_cstring];

    memcpy(base, source, src_len);
    next_temp_cstring += src_len;

    return (byte_slice){base, src_len};
}

byte_slice make_cstring(lexer_t *lexer, byte_slice source) {
    u8 *base = &lexer->string_stack[lexer->next_string];
    size_t real_length = source.len + 1;
    if (lexer->next_string + real_length <= lexer->stack_size) {
        memcpy(base, source.at, source.len);
        base[real_length - 1] = '\0';
        lexer->next_string += real_length;
    } else {
        panic("unable to allocate cstring");
    }
}

#define CSTR(s) temp_cstring(s)

byte_slice stringify_token(lexer_t *lexer, token_t t) {
    switch (t.kind) {
    case T_NONE:
        return CSTR("NONE");
    case T_SIMPLE:
        return CSTR("T_SIMPLE");
    case T_LEFT_BRACKET:
        return CSTR("LEFT_BRACKET");
    case T_RIGHT_BRACKET:
        return CSTR("RIGHT_BRACKET");
    case T_LEFT_CURLY:
        return CSTR("LEFT_CURLY");
    case T_RIGHT_CURLY:
        return CSTR("RIGHT_CURLY");
    case T_COMMA:
        return CSTR("COMMA");
    case T_COLON:
        return CSTR("COLON");
    case T_MINUS:
        return CSTR("MINUS");
    case T_TRUE:
        return CSTR("TRUE");
    case T_FALSE:
        return CSTR("FALSE");
    case T_STRING_LIT: {
        __fmt("STR_LIT(%s)", t.string_lit.at);
        return make_cstring(
            lexer, (byte_slice){formatted_string, strlen(formatted_string)});
    } break;

    case T_IDENTIFIER: {
        __fmt("IDENT(%s)", t.string_lit.at);
        return make_cstring(
            lexer, (byte_slice){formatted_string, strlen(formatted_string)});
    } break;

    case T_NUMBER_LIT:
    default:
        panic("num literal to string");
        break;
    }
}

token_error tokenize(lexer_t *lexer, byte_slice tokens) {
    u8 c;
    size_t next_token = 0;

    while (lexer->position < lexer->len) {
        c = consume(lexer);
        lexer->begin_i = lexer->position - 1;

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
                // number
            } else if (((type = map_char[c]) & T_SIMPLE) == T_SIMPLE) {
                if (!push_token((token_t){type, c}, tokens.at, next_token++,
                                tokens.len)) {

                    panic("ran out of space for tokens");
                }
            } else {
                while (match_consume_alphanumeric(lexer)) {
                }
                size_t len = lexer->position - lexer->begin_i;
                byte_slice string = make_cstring(
                    lexer, (byte_slice){&lexer->bytes[lexer->begin_i], len});

                byte_slice interned_string = intern(lexer, string);
                token_t offender = (token_t){.kind = T_IDENTIFIER,
                                             .identifier = interned_string};
                return (token_error){TE_UNKNOWN_TOKEN, offender};
            }
        }
    }

    return (token_error){.kind = TE_NONE};
}

int main(int argc, char const *argv[]) {
    dbg("sizeof token: %zu bytes", sizeof(token_t));

    // NOTE(yousef): arguments to main are validated somewhere else.
    char const *filename = argv[1];
    size_t file_size = (size_t)atoi(argv[2]); // in bytes

    size_t pool_size = file_size + MiB(300u);
    u8 *reserved_memory_pool;
    if (!alloc_commit(pool_size, &reserved_memory_pool)) {
        panic("unable to allocate required memory at start up");
    }

    u8 *input_buffer = reserved_memory_pool;
    size_t input_buffer_size = file_size;
    byte_slice slice = (byte_slice){input_buffer, input_buffer_size};
    read_file(filename, &slice);

    token_t *token_buffer =
        (token_t *)(&reserved_memory_pool[input_buffer_size]);
    size_t token_buffer_size = MiB(40u);

    u8 *string_pool = (u8 *)(&token_buffer[token_buffer_size]);
    size_t string_pool_size = MiB(100u);

    lexer_t lexer = (lexer_t){
        .bytes = input_buffer,
        .len = input_buffer_size,
        .position = 0,
        .begin_i = 0,
        .string_table = NULL,

        .string_stack = string_pool,
        .next_string = 0,
        .stack_size = string_pool_size,
    };

    token_error error =
        tokenize(&lexer, (byte_slice){.at = (u8 *)token_buffer,
                                      .len = token_buffer_size});
    if (error.kind != TE_NONE) {
        u8 *to_string = stringify_token(&lexer, error.token).at;
        panic("error: token is %s", to_string);
    }
}
