#include "common.h"
#include "platform.c"
#include <assert.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

typedef enum token_type {
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
    T_NULL = T_SIMPLE | 0x200u,

    T_NUMBER_LIT = 1,
    T_STRING_LIT = 2,
    T_UNKNOWN = 3,
    T_UNTERMINATED_STRING_LIT = 4,
    T_EOF = 0xFFFFFFFF,
} token_type_t;

typedef struct token {
    enum token_type kind;
    union {
        struct {
            char simple_token;
        };
        byte_slice byte_sequence; // used for unidentified tokens as well
    };
} token_t;

typedef struct lexer {
    u8 const *bytes;
    size_t len;
    size_t position;
    size_t begin_i;

    token_t *tokens;
    size_t max_tokens;
    size_t next_token;
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

typedef struct parser {
    token_t *tokens;
    size_t len;
    size_t position;
} parser_t;

typedef enum value_type {
    J_NONE = 0,
    J_OBJECT,
    J_ARRAY,
    J_STRING,
    J_NUMBER,
    J_TRUE,
    J_FALSE,
    J_NULL,
} jvalue_type_t;

typedef struct json_result {
    token_t offender;
    // TODO(yousef): later byte and line position in original file
} json_result_t;

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

bool push_token(lexer_t *lexer, token_t t) {
    size_t at = lexer->next_token;
    if (at + 1 <= lexer->max_tokens) {
        lexer->tokens[at] = t;
        lexer->next_token += 1;
        return true;
    }
    return false;
}

#define STR(src) intern_cstring(src)
#define CSTR(src) (intern_cstring(src).at)

#define SLICE(src, len) ((byte_slice){src, len})
#define INTERN(slice) intern_string(slice)

byte_slice intern_string(byte_slice source) {
    u8 *base = interner.string_stack + interner.next_string;
    size_t temp_next_string = interner.next_string;

    size_t real_length = source.len + 1;
    if (interner.next_string + real_length <= interner.stack_size) {
        memcpy(base, source.at, source.len);
        base[real_length - 1] = '\0';
        interner.next_string += real_length;
    } else {
        panic("unable to allocate cstring");
    }
    byte_slice allocated = {base, real_length};
    byte_slice slice;

    if ((slice = shget(STRING_TABLE, allocated.at)).at != NULL) {
        // string already stored: discard temp allocation
        interner.next_string = temp_next_string;
        return slice;
    } else {
        return shput(STRING_TABLE, allocated.at, allocated);
    }
}

byte_slice intern_cstring(char const *string) {
    byte_slice slice = (byte_slice){string, strlen(string)};
    return intern_string(slice);
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

char const *format_jvalue(jvalue_type_t kind) {
    switch (kind) {
    case J_TRUE:
        return CSTR("TRUE");
    case J_FALSE:
        return CSTR("FALSE");
    case J_NULL:
        return CSTR("NULL");

    case J_ARRAY:
        return CSTR("ARRAY[...]");

    case J_OBJECT:
        return CSTR("OBJ{...}");

    case J_STRING:
        return CSTR("STRING");

    case J_NUMBER:
        return CSTR("NUMBER");
    }
}

char const *format_token(token_t t) {
    switch (t.kind) {
    case T_EOF:
        return CSTR("EOF");
    case T_NONE:
        return CSTR("T_NONE");
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
    case T_NULL:
        return CSTR("NULL");

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

    case T_NUMBER_LIT: {
        __fmt("NUMBER(%s)", t.byte_sequence.at);
        return intern_string(
                   (byte_slice){formatted_string, strlen(formatted_string)})
            .at;
    } break;
    }
}

bool match_consume_digit(lexer_t *lexer) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return false;
    }

    u8 c = lexer->bytes[pos];
    if (c >= '0' && c <= '9') {
        lexer->position += 1;
        return true;
    }

    return false;
}

bool match_consume_nonzero(lexer_t *lexer) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return false;
    }

    u8 c = lexer->bytes[pos];
    if (c >= '1' && c <= '9') {
        lexer->position += 1;
        return true;
    }

    return false;
}

typedef enum result {
    RES_NONE,
    RES_SOME,
    RES_ERROR,
} result_t;

result_t match_fraction(lexer_t *lexer) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return RES_NONE;
    }

    u8 c = lexer->bytes[pos];

    if (c == '.') {
        lexer->position += 1;
        if (!match_consume_digit(lexer)) {
            return RES_ERROR;
        }

        while (match_consume_digit(lexer)) {
        }

        return RES_SOME;
    }
    return RES_NONE;
}

result_t match_exponent(lexer_t *lexer) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return RES_NONE;
    }

    u8 c = lexer->bytes[pos];
    if (c == 'E' || c == 'e') {
        lexer->position += 1;

        if (match_consume_digit(lexer)) {
            goto resume;
        } else {
            u8 k = consume(lexer);
            if (k == '+' || k == '-') {
                if (match_consume_digit(lexer)) {
                    goto resume;
                }
            }

            return RES_ERROR;
        }

    resume:
        while (match_consume_digit(lexer)) {
        }
        return RES_SOME;
    }
    return RES_NONE;
}

void tokenize(lexer_t *lexer) {
    u8 c;
    size_t next_token = 0;

    while (lexer->position < lexer->len) {
        c = consume(lexer);
        lexer->begin_i = lexer->position - 1;

        switch (c) {
        case '\0':
            assert(lexer->position == lexer->len - 1);
            break;

        case ' ':
        case '\n':
        case '\r':
        case '\t':
            // ignore whitespace
            break;

        case 't':
        case 'f':
        case 'n': {
            token_type_t expected_type =
                c == 't' ? T_TRUE : (c == 'f' ? T_FALSE : T_NULL);

            byte_slice expect =
                expected_type == T_TRUE
                    ? STR("true")
                    : (expected_type == T_FALSE ? STR("false") : STR("null"));

            while (MATCH_CONSUME_IDENT_CHAR(lexer)) {
            }
            size_t len = lexer->position - lexer->begin_i;
            byte_slice string =
                INTERN(SLICE(lexer->bytes + lexer->begin_i, len));

            if (equals(string, expect)) {
                if (!push_token(lexer, (token_t){expected_type,
                                                 .byte_sequence = expect})) {

                    panic("ran out of space for tokens");
                }
            } else {
                // TODO(yousef): push offending token?
                return;
            }
        } break;

        case '"': {
            while (match_consume_any_strchar(lexer)) {
            };
            u8 last = consume(lexer);

            size_t start = lexer->begin_i + 1;
            size_t len = lexer->position - start - 1;
            byte_slice slice = INTERN(SLICE(lexer->bytes + start, len));

            switch (last) {
            case '"':
                token_t t = (token_t){T_STRING_LIT, .byte_sequence = slice};
                push_token(lexer, t);
                break;
            case '\\':
                todo("escape sequences, et cetera.");
                break;
            case '\0':
                return;
            default:
                panic("unreachable code path");
            }
        } break;

        // number = integer fraction exponent
        // integer = digit | nonzero digits
        // digits = digit | digit digits
        // digit = '0' | nonzero
        // nonzero = '1' | '2' | ... | '9'
        // fraction = epsilon | '.' digits
        // exponent = epsilon | ('E' | 'e') sign digits
        // sign = epsilon | '-' | '+'
        case '0': {
            if (match_consume_digit(lexer)) {
                // 01 (et cetera) not allowed
                return;
            }
        } break;

        default: {
            token_type_t type;
            if (c >= '1' && c <= '9') {
                while (match_consume_digit(lexer)) {
                }
                if (match_fraction(lexer) == RES_ERROR) {
                    return;
                }

                if (match_exponent(lexer) == RES_ERROR) {
                    return;
                }
                size_t len = lexer->position - lexer->begin_i;
                byte_slice slice =
                    INTERN(SLICE(lexer->bytes + lexer->begin_i, len));
                token_t t = {
                    .kind = T_NUMBER_LIT,
                    .byte_sequence = slice,
                };
                push_token(lexer, t);
            } else if (((type = map_char[c]) & T_SIMPLE) == T_SIMPLE) {
                token_t t = (token_t){type, c};
                if (!push_token(lexer, t)) {
                    panic("ran out of space for tokens");
                }
            } else {
                while (MATCH_CONSUME_IDENT_CHAR(lexer)) {
                }
                size_t len = lexer->position - lexer->begin_i;
                byte_slice slice =
                    INTERN(SLICE(lexer->bytes + lexer->begin_i, len));

                return;
            }
        } break;
        }
    }

    push_token(lexer, (token_t){.kind = T_EOF});
}

token_t peek_token(parser_t *parser) {
    assert(parser->tokens[parser->len - 1].kind == T_EOF);

    size_t pos = parser->position;
    if (pos >= parser->len) {
        return parser->tokens[parser->len - 1];
    }
    return parser->tokens[pos];
}

bool consume_token(parser_t *parser, token_type_t expect) {
    assert(parser->tokens[parser->len - 1].kind == T_EOF);

    size_t pos = parser->position;
    if (pos >= parser->len) {
        return false;
    }
    if (parser->tokens[pos].kind == expect) {
        parser->position += 1;
        return true;
    }
    return false;
}

void advance(parser_t *parser) { parser->position++; }

jvalue_type_t parse_value(parser_t *parser) {
    assert(parser->tokens[parser->len - 1].kind == T_EOF);
    token_t token = peek_token(parser);
    dbg("token: %s", format_token(token));

    switch (token.kind) {
    // obj
    case T_LEFT_CURLY: {
        advance(parser);
        while (consume_token(parser, T_STRING_LIT)) {
            if (!consume_token(parser, T_COLON)) {
                return J_NONE; // for now just exit function completely,
                               // later do better error handling
            }
            jvalue_type_t val = parse_value(parser);
            if (val == J_NONE) {
                return J_NONE;
            }
            if (!consume_token(parser, T_COMMA)) {
                break;
            }
        }
        if (!consume_token(parser, T_RIGHT_CURLY)) {
            return J_NONE;
        }

        return J_OBJECT;
    } break;

    // array
    case T_LEFT_BRACKET: {
        advance(parser);
        while (parse_value(parser) != J_NONE) {
            if (!consume_token(parser, T_COMMA)) {
                break;
            }
        }
        if (!consume_token(parser, T_RIGHT_BRACKET)) {
            return J_NONE;
        }

        return J_ARRAY;
    } break;

    // string
    case T_STRING_LIT:
        advance(parser);
        return J_STRING;

    // numbers
    case T_MINUS:
        advance(parser);
        if (!consume_token(parser, T_NUMBER_LIT)) {
            return J_NONE;
        }
        token_t num = parser->tokens[parser->position - 1];
        return J_NUMBER;

    case T_NUMBER_LIT:
        advance(parser);
        return J_NUMBER;

    case T_TRUE:
        advance(parser);
        return J_TRUE;

    case T_FALSE:
        advance(parser);
        return J_FALSE;

    case T_NULL:
        advance(parser);
        return J_NULL;

    default:
        return J_NONE;
    }
}

jvalue_type_t parse(parser_t *parser) { return parse_value(parser); }

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

    byte_slice file_data = {input_buffer, input_buffer_size};
    read_file(filename, &file_data);

    lexer_t lexer = {.bytes = file_data.at,
                     .len = file_data.len,
                     .position = 0,
                     .begin_i = 0,
                     .tokens = (token_t *)token_buffer,
                     .max_tokens = token_buffer_size / sizeof(token_t),
                     .next_token = 0};

    tokenize(&lexer);
    size_t token_count = lexer.next_token;
    token_t last = ((token_t *)token_buffer)[token_count - 1];
    if (last.kind != T_EOF) {
        u8 *to_string = format_token(last);
        panic("token %s", to_string);
    }

    for (int i = 0; i < token_count; ++i) {
        token_t e = ((token_t *)token_buffer)[i];
        char *formatted = format_token(e);
        dbg("tokens[%d]: %s", i, formatted);
    }
    parser_t parser = {
        .tokens = token_buffer, .len = token_count, .position = 0};

    jvalue_type_t val = parse(&parser);
    dbg(format_jvalue(val));
}
