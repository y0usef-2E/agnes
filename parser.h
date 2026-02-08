#if !defined(AG_PARSER_H)
#define AG_PARSER_H

#include "common.h"
#include <assert.h>

#define AG_INTERNER_IMPLEMENT
#include "interner.h"

typedef enum token_type {
    T_NONE = 0u,
    T_SIMPLE = 0x8000u,

    T_LEFT_BRACKET = T_SIMPLE | 0x1u,
    T_RIGHT_BRACKET = T_SIMPLE | 0x2u,
    T_LEFT_CURLY = T_SIMPLE | 0x4u,
    T_RIGHT_CURLY = T_SIMPLE | 0x8u,
    T_COMMA = T_SIMPLE | 0x10u,
    T_COLON = T_SIMPLE | 0x20u,

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
    struct {
        char const *filename;
    };
    u8 const *bytes;
    size_t len;
    size_t position;
    size_t begin_i;

    token_t *tokens;
    size_t max_tokens;
    size_t next_token;

    size_t *line_info;
    size_t current_line;
} lexer_t;

typedef struct parser {
    struct {
        char const *filename;
    };
    token_t *tokens;
    size_t len;
    size_t position;
} parser_t;

typedef enum jvalue_kind {
    J_NONE = 0,
    J_OBJECT,
    J_ARRAY,
    J_STRING,
    J_NUMBER,
    J_TRUE,
    J_FALSE,
    J_NULL,
    J_ERROR,
} jvalue_kind_t;

enum agnes_res_kind {
    RES_NONE = 0,
    RES_LEXER_NONE = RES_NONE,
    RES_PARSER_NONE = RES_NONE,

    RES_LEXER_SOME,
    RES_LEXER_ERROR,

    RES_PARSER_ERROR,
    RES_PARSER_SOME,

    RES_OUT_OF_SPACE = 0xFFFF,
};

typedef struct agnes_result {
    enum agnes_res_kind kind;
    size_t byte_pos;
    size_t line;
    union {
        token_t fragment;
        jvalue_kind_t jvalue;
    };
} agnes_result_t;

typedef struct agnes_parser {
    char const *filename;
    u8 const *bytes;
    size_t file_size;

    token_t *tokens;
    size_t max_tokens;

    size_t *line_info;

    u8 *string_pool;
    size_t string_pool_size;
} agnes_parser_t;

// 'public' API
static agnes_result_t parse_json(agnes_parser_t *agnes_parser);

// implementation
#if defined(AG_PARSER_IMPLEMENT)

static u8 *format_jvalue(jvalue_kind_t kind) {
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

static u8 *format_token(token_t t) {
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

static enum token_type map_char[256] = {
    [':'] = T_COLON,         [','] = T_COMMA,      ['['] = T_LEFT_BRACKET,
    [']'] = T_RIGHT_BRACKET, ['{'] = T_LEFT_CURLY, ['}'] = T_RIGHT_CURLY,
};

static u8 consume(lexer_t *lexer) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return '\0';
    }
    lexer->position += 1;
    return lexer->bytes[pos];
}

static bool push_token(lexer_t *lexer, token_t t) {
    size_t at = lexer->next_token;
    if (at + 1 <= lexer->max_tokens) {
        lexer->tokens[at] = t;
        lexer->next_token += 1;
        lexer->line_info[at] = lexer->current_line;
        return true;
    }
    return false;
}

#define MATCH_CONSUME_ALPHANUMERIC(lexer) match_consume_ident_char(lexer, false)

#define MATCH_CONSUME_IDENT_CHAR(lexer) match_consume_ident_char(lexer, true)

static bool match_consume_any_strchar(lexer_t *lexer) {
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

static bool match_consume_ident_char(lexer_t *lexer, bool with_underscore) {
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

#define MATCH_CONSUME_ZERO(lexer) match_consume_digit(lexer, '0', '0')
#define MATCH_CONSUME_NONZERO(lexer) match_consume_digit(lexer, '1', '9')
#define MATCH_CONSUME_ANY_DIGIT(lexer) match_consume_digit(lexer, '0', '9')

static bool match_consume_digit(lexer_t *lexer, u8 start, u8 end) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return false;
    }

    u8 c = lexer->bytes[pos];
    if (c >= start && c <= end) {
        lexer->position += 1;
        return true;
    }

    return false;
}

#define LEXER_OUT_OF_SPACE ((agnes_result_t){RES_OUT_OF_SPACE})

static enum agnes_res_kind match_fraction(lexer_t *lexer) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return RES_LEXER_NONE;
    }

    u8 c = lexer->bytes[pos];

    if (c == '.') {
        lexer->position += 1;
        if (!MATCH_CONSUME_ANY_DIGIT(lexer)) {
            return RES_LEXER_ERROR;
        }

        while (MATCH_CONSUME_ANY_DIGIT(lexer)) {
        }

        return RES_LEXER_SOME;
    }
    return RES_LEXER_NONE;
}

static enum agnes_res_kind match_exponent(lexer_t *lexer) {
    size_t pos = lexer->position;
    if (pos >= lexer->len) {
        return RES_LEXER_NONE;
    }

    u8 c = lexer->bytes[pos];
    if (c == 'E' || c == 'e') {
        lexer->position += 1;

        if (MATCH_CONSUME_ANY_DIGIT(lexer)) {
            goto resume;
        } else {
            u8 k = consume(lexer);
            if (k == '+' || k == '-') {
                if (MATCH_CONSUME_ANY_DIGIT(lexer)) {
                    goto resume;
                }
            }

            return RES_LEXER_ERROR;
        }

    resume:
        while (MATCH_CONSUME_ANY_DIGIT(lexer)) {
        }
        return RES_LEXER_SOME;
    }
    return RES_LEXER_NONE;
}

static agnes_result_t token_error(lexer_t *lexer, token_type_t token_kind) {
    size_t len = lexer->position - lexer->begin_i;
    token_t t = {.kind = token_kind,
                 .byte_sequence =
                     INTERN(SLICE(lexer->bytes + lexer->begin_i, len))};

    return (agnes_result_t){RES_LEXER_ERROR, lexer->begin_i,
                            lexer->current_line, t};
}

static agnes_result_t tokenize(lexer_t *lexer) {
    u8 c;
    size_t next_token = 0;
    size_t line_number = 1;

    while (lexer->position < lexer->len) {
        c = consume(lexer);
        lexer->begin_i = lexer->position - 1;

        switch (c) {
        case '-':
            if (lexer->len > lexer->position) {
                if (MATCH_CONSUME_NONZERO(lexer)) {
                    goto lex_one_to_nine;
                } else if (MATCH_CONSUME_ZERO(lexer)) {
                    goto lex_zero;
                }
            }
            return token_error(lexer, T_NUMBER_LIT);

        case '\0':
            return token_error(lexer, T_UNKNOWN);

        case '\n':
            lexer->current_line++;
            break;
        case ' ':
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

            if (str_eq(string, expect)) {
                if (!push_token(lexer, (token_t){expected_type,
                                                 .byte_sequence = expect})) {
                    return LEXER_OUT_OF_SPACE;
                }
            } else {
                return token_error(lexer, T_UNKNOWN);
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
                if (!push_token(lexer, t)) {
                    return LEXER_OUT_OF_SPACE;
                }
                break;
            case '\\':
                todo("escape sequences, et cetera.");
                break;
            case '\0':
                return token_error(lexer, T_UNTERMINATED_STRING_LIT);
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
        lex_zero:
            if (MATCH_CONSUME_ANY_DIGIT(lexer)) {
                return token_error(lexer, T_NUMBER_LIT);
            }
            if (match_fraction(lexer) == RES_LEXER_ERROR) {
                return token_error(lexer, T_NUMBER_LIT);
            }
            if (match_exponent(lexer) == RES_LEXER_ERROR) {
                return token_error(lexer, T_NUMBER_LIT);
            }

            size_t len = lexer->position - lexer->begin_i;
            byte_slice slice =
                INTERN(SLICE(lexer->bytes + lexer->begin_i, len));
            token_t t = {
                .kind = T_NUMBER_LIT,
                .byte_sequence = slice,
            };
            if (!push_token(lexer, t)) {
                return LEXER_OUT_OF_SPACE;
            }
            break;
        }

        default: {
            token_type_t type;
            if (c >= '1' && c <= '9') {
            lex_one_to_nine:
                while (MATCH_CONSUME_ANY_DIGIT(lexer)) {
                }

                if (match_fraction(lexer) == RES_LEXER_ERROR) {
                    return token_error(lexer, T_NUMBER_LIT);
                }

                if (match_exponent(lexer) == RES_LEXER_ERROR) {
                    return token_error(lexer, T_NUMBER_LIT);
                }
                size_t len = lexer->position - lexer->begin_i;
                byte_slice slice =
                    INTERN(SLICE(lexer->bytes + lexer->begin_i, len));

                token_t t = {
                    .kind = T_NUMBER_LIT,
                    .byte_sequence = slice,
                };
                if (!push_token(lexer, t)) {
                    return LEXER_OUT_OF_SPACE;
                }

            } else if (((type = map_char[c]) & T_SIMPLE) == T_SIMPLE) {
                token_t t = (token_t){type, c};
                if (!push_token(lexer, t)) {
                    return LEXER_OUT_OF_SPACE;
                }
            } else {
                while (MATCH_CONSUME_IDENT_CHAR(lexer)) {
                }

                return token_error(lexer, T_UNKNOWN);
            }
        } break;
        }
    }

    if (!push_token(lexer, (token_t){.kind = T_EOF})) {
        return LEXER_OUT_OF_SPACE;
    }
    return (agnes_result_t){
        RES_LEXER_NONE,
    };
}

static token_t peek_token(parser_t *parser) {
    assert(parser->tokens[parser->len - 1].kind == T_EOF);

    size_t pos = parser->position;
    if (pos >= parser->len) {
        return parser->tokens[parser->len - 1];
    }
    return parser->tokens[pos];
}

static bool consume_token(parser_t *parser, token_type_t expect) {
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

static void advance(parser_t *parser) { parser->position++; }

static jvalue_kind_t parse_value(parser_t *parser) {
    assert(parser->tokens[parser->len - 1].kind == T_EOF);
    token_t token = peek_token(parser);
    // dbg("token: %s", format_token(token));

    switch (token.kind) {
    // obj
    case T_LEFT_CURLY: {
        advance(parser);
        bool last_is_pair = false;
        bool consumed_pair = false;
        while (consume_token(parser, T_STRING_LIT)) {
            if (!consume_token(parser, T_COLON)) {
                return J_ERROR; // for now just exit function completely,
                                // later do better error handling
            }
            jvalue_kind_t val = parse_value(parser);
            if (val == J_ERROR || val == J_NONE) {
                return J_ERROR;
            }

            consumed_pair = true;
            last_is_pair = true;
            if (!consume_token(parser, T_COMMA)) {
                break;
            }
            last_is_pair = false;
        }
        if (last_is_pair || !consumed_pair) {
            if (!consume_token(parser, T_RIGHT_CURLY)) {
                return J_ERROR;
            }
            return J_OBJECT;
        } else {
            return J_ERROR;
        }
    } break;

    // array
    case T_LEFT_BRACKET: {
        advance(parser);
        bool last_is_val = false;
        bool consumed_val = false;
        while (parse_value(parser) != J_NONE) {
            consumed_val = true;
            last_is_val = true;
            if (!consume_token(parser, T_COMMA)) {
                break;
            }
            last_is_val = false;
        }
        if (last_is_val || !consumed_val) {
            if (!consume_token(parser, T_RIGHT_BRACKET)) {
                return J_ERROR;
            }
            return J_ARRAY;
        } else {
            return J_ERROR;
        }
    } break;

    // string
    case T_STRING_LIT:
        advance(parser);
        return J_STRING;

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

static agnes_result_t parse_json(agnes_parser_t *agnes_parser) {
    lexer_t lexer = {
        .filename = agnes_parser->filename,
        .bytes = agnes_parser->bytes,
        .len = agnes_parser->file_size,
        .position = 0,
        .begin_i = 0,
        .tokens = agnes_parser->tokens,
        .max_tokens = agnes_parser->max_tokens,
        .next_token = 0,

        .line_info = agnes_parser->line_info,
        .current_line = 1,
    };

    init_global_interner(agnes_parser->string_pool,
                         agnes_parser->string_pool_size);

    agnes_result_t res = tokenize(&lexer);
    if (res.kind == RES_LEXER_ERROR) {
        return res;
    }

    parser_t parser = {.filename = lexer.filename,
                       .tokens = lexer.tokens,
                       .len = lexer.next_token,
                       .position = 0};

    // TODO(yousef): make this check more friendly
    if (parser.len < 1) {
        panic("file=%s: parser->len<1", parser.filename);
    }
    if (parser.tokens[0].kind == T_EOF) {
        return (agnes_result_t){.kind = RES_PARSER_NONE};
    }

    jvalue_kind_t v = parse_value(&parser);

    if (!consume_token(&parser, T_EOF) || v == J_ERROR) {
        return (agnes_result_t){.kind = RES_PARSER_ERROR};
    }

    return (agnes_result_t){.kind = RES_PARSER_SOME, .jvalue = v};
}

#endif
#endif