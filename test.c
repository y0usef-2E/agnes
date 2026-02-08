#define AG_PARSER_IMPLEMENT
#include "parser.h"
#include "platform.c"

// MAYBE(yousef): hash table implementation.
// TODO(yousef): public API.
// TODO(yousef): testing.
int main(int argc, char const *argv[]) {
    dbg("sizeof token: %zu bytes", sizeof(token_t));

    char const *filename = argv[1];
    size_t max_file_size = MiB(600u);

    size_t pool_size = max_file_size + MiB(300u);
    u8 *reserved_memory_pool;
    if (!alloc_commit(pool_size, &reserved_memory_pool)) {
        panic("unable to allocate required memory at start up");
    }

    u8 *input_buffer = reserved_memory_pool;
    size_t input_buffer_size = max_file_size;

    u8 *token_buffer = reserved_memory_pool + input_buffer_size;
    size_t token_buffer_size = MiB(40u);

    u8 *string_pool = token_buffer + token_buffer_size;
    size_t string_pool_size = MiB(100u);

    byte_slice file_data = {input_buffer, input_buffer_size};
    read_file(filename, &file_data);

    u8 *line_info_buffer = string_pool + string_pool_size;
    size_t line_info_buffer_size =
        (token_buffer_size / sizeof(token_t)) * sizeof(size_t);

    agnes_parser_t parser = {0};
    parser.bytes = input_buffer;
    parser.file_size = file_data.len;
    parser.line_info = (size_t *)line_info_buffer;
    parser.filename = filename;
    parser.max_tokens = token_buffer_size / sizeof(token_t);
    parser.tokens = (token_t *)token_buffer;
    parser.string_pool = string_pool;
    parser.string_pool_size = string_pool_size;

    agnes_result_t result = parse_json(&parser);

    if (result.kind == RES_PARSER_ERROR || result.kind == RES_LEXER_ERROR) {
        return EXIT_FAILURE;
    }
    dbg("got: %s", format_jvalue(result.jvalue));
    return EXIT_SUCCESS;
}