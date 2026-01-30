#include "common.h"
#include "platform.c"
#include <assert.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

typedef enum token_type {
    T_NONE = 0u,
    T_EOF = T_NONE,

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
    T_UNKNOWN = 3,
    T_UNTERMINATED_STRING_LIT,
} token_type_t;

typedef struct number_lit {
    union {
        u64 integral;
        double floating;
    };
} number_lit_t;

typedef struct token {
    enum token_type kind;
    union {
        struct {
            char simple_token;
        };
        struct number_lit num;
        byte_slice byte_sequence; // used for unidentified tokens
    };
} token_t;

typedef struct lexer {
    u8 const *bytes;
    size_t len;
    size_t position;
    size_t begin_i;
} lexer_t;

typedef struct string_interner {
    struct {
        char *key;
        byte_slice value;
    } *string_table;
    u8 *string_stack;
    size_t next_string;
    size_t stack_size;
} interner_t;

static interner_t interner = {0};
#define STRING_TABLE interner.string_table

static enum token_type map_char[256] = {
    [':'] = T_COLON,         [','] = T_COMMA,      ['['] = T_LEFT_BRACKET,
    [']'] = T_RIGHT_BRACKET, ['{'] = T_LEFT_CURLY, ['}'] = T_RIGHT_CURLY,
    ['-'] = T_MINUS};

u8 consume(lexer_t *lexer) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return '\0';
    }
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

#define STR(src) (intern_cstring(src))

byte_slice intern_cstring(char const *string) {
    byte_slice slice;
    if ((slice = shget(STRING_TABLE, string)).at != NULL) {
        return slice;
    } else {
        slice = (byte_slice){string, strlen(string)};
        return shput(STRING_TABLE, string, slice);
    }
}

#define SLICE(src, len) ((byte_slice){src, len})
#define INTERN(slice) intern_string(slice)

byte_slice intern_string(byte_slice source) {
    u8 *base = interner.string_stack + interner.next_string;
    size_t real_length = source.len + 1;
    if (interner.next_string + real_length <= interner.stack_size) {
        memcpy(base, source.at, source.len);
        base[real_length - 1] = '\0';
        interner.next_string += real_length;
    } else {
        panic("unable to allocate cstring");
    }
    byte_slice allocated = (byte_slice){base, real_length};
    byte_slice slice;
    if ((slice = shget(STRING_TABLE, allocated.at)).at != NULL) {
        return slice;
    } else {
        return shput(STRING_TABLE, allocated.at, allocated);
    }
}

bool equals(byte_slice left, byte_slice right) { return left.at == right.at; }

#define MATCH_CONSUME_ALPHANUMERIC(lexer) match_consume_ident_char(lexer, false)

#define MATCH_CONSUME_IDENT_CHAR(lexer) match_consume_ident_char(lexer, true)

bool match_consume_any_strchar(lexer_t *lexer) {
    size_t pos;
    if ((pos = lexer->position) >= lexer->len) {
        return false;
    }
    u8 c = lexer->bytes[pos];
    if (c == '\\' || c == '"') {
        return false;
    }
    lexer->position += 1;
    return true;
}

bool match_consume_ident_char(lexer_t *lexer, bool with_underscore) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return false;
    }
    u8 c = lexer->bytes[pos];
    if (c == '_' && with_underscore) {
        lexer->position += 1;
        return true;
    } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9')) {
        lexer->position += 1;
        return true;
    }
    return false;
}

char const *stringify_token(lexer_t *lexer, token_t t) {
    switch (t.kind) {
    case T_NONE:
        return "NONE";
    case T_SIMPLE:
        return "T_SIMPLE";
    case T_LEFT_BRACKET:
        return "LEFT_BRACKET";
    case T_RIGHT_BRACKET:
        return "RIGHT_BRACKET";
    case T_LEFT_CURLY:
        return "LEFT_CURLY";
    case T_RIGHT_CURLY:
        return "RIGHT_CURLY";
    case T_COMMA:
        return "COMMA";
    case T_COLON:
        return "COLON";
    case T_MINUS:
        return "MINUS";
    case T_TRUE:
        return "TRUE";
    case T_FALSE:
        return "FALSE";

    case T_STRING_LIT: {
        __fmt("STR_LIT(%s)", t.byte_sequence.at);
        return intern_string(
                   (byte_slice){formatted_string, strlen(formatted_string)})
            .at;
    } break;

    case T_UNKNOWN: {
        __fmt("UNKNOWN(%s)", t.byte_sequence.at);
        return intern_string(
                   (byte_slice){formatted_string, strlen(formatted_string)})
            .at;
    } break;

    case T_UNTERMINATED_STRING_LIT: {
        __fmt("UNTERMINATED_STR(%s)", t.byte_sequence.at);
        return intern_string(
                   (byte_slice){formatted_string, strlen(formatted_string)})
            .at;
    } break;

    case T_NUMBER_LIT:
    default:
        todo("stringify token=%d", t.kind);
        break;
    }
}

token_t tokenize(lexer_t *lexer, byte_slice tokens) {
    u8 c;
    size_t next_token = 0;

    while (lexer->position < lexer->len) {
        c = consume(lexer);
        lexer->begin_i = lexer->position - 1;

        switch (c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            // ignore whitespace
            break;

        case 't':
        case 'f': {
            token_type_t type;

            bool consumed_t = c == 't';
            byte_slice expect = consumed_t ? STR("true") : STR("false");

            while (MATCH_CONSUME_IDENT_CHAR(lexer)) {
            }
            size_t len = lexer->position - lexer->begin_i;
            byte_slice string =
                INTERN(SLICE(lexer->bytes + lexer->begin_i, len));

            if (equals(string, expect)) {
                if (!push_token((token_t){type, c}, tokens.at, next_token++,
                                tokens.len)) {

                    panic("ran out of space for tokens");
                }
            } else {
                return (token_t){.kind = T_UNKNOWN, .byte_sequence = string};
            }
        } break;

        case '"': {
            while (match_consume_any_strchar(lexer)) {
            };
            u8 last = consume(lexer);

            size_t start = lexer->begin_i + 1;
            size_t len = lexer->position - start;
            byte_slice slice = INTERN(SLICE(lexer->bytes + start, len));

            switch (last) {
            case '"':
                token_t t = (token_t){T_STRING_LIT, .byte_sequence = slice};
                push_token(t, tokens.at, next_token++, tokens.len);
                break;
            case '\\':
                todo("escape sequences, et cetera.");
                break;
            case '\0':
                return (token_t){T_UNTERMINATED_STRING_LIT,
                                 .byte_sequence = slice};
            default:
                panic("unreachable code path");
            }
        } break;

        default: {
            token_type_t type;
            if (c >= '0' && c <= '9') {
                // number
            } else if (((type = map_char[c]) & T_SIMPLE) == T_SIMPLE) {
                if (!push_token((token_t){type, c}, tokens.at, next_token++,
                                tokens.len)) {

                    panic("ran out of space for tokens");
                }
            } else {
                while (MATCH_CONSUME_IDENT_CHAR(lexer)) {
                }
                size_t len = lexer->position - lexer->begin_i;
                byte_slice slice =
                    INTERN(SLICE(lexer->bytes + lexer->begin_i, len));

                return (token_t){.kind = T_UNKNOWN, .byte_sequence = slice};
            }
        } break;
        }
    }

    return (token_t){.kind = T_NONE};
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

    u8 *token_buffer = reserved_memory_pool + input_buffer_size;
    size_t token_buffer_size = MiB(40u);

    u8 *string_pool = token_buffer + token_buffer_size;
    size_t string_pool_size = MiB(100u);

    interner.string_table = NULL;
    interner.string_stack = string_pool;
    interner.stack_size = string_pool_size;
    interner.next_string = 0;

    byte_slice slice = (byte_slice){input_buffer, input_buffer_size};
    read_file(filename, &slice);

    lexer_t lexer = (lexer_t){
        .bytes = input_buffer,
        .len = input_buffer_size,
        .position = 0,
        .begin_i = 0,

    };
    token_t last = tokenize(&lexer, (byte_slice){.at = (u8 *)token_buffer,
                                                 .len = token_buffer_size});
    if (last.kind != T_EOF) {
        u8 *to_string = stringify_token(&lexer, last);
        panic("token %s", to_string);
    }
}
