#define AG_PARSER_IMPLEMENT
#include "parser.h"
#include "platform.c"

// example wrapper functions
bool stupid_alloc(size_t size, u8 **out) {
    u8 *ptr = (u8 *)malloc(size);
    if (ptr == NULL) {
        return false;
    } else {
        *out = ptr;
        return true;
    }
}

bool stupid_realloc(size_t new_size, u8 *in, u8 **out) {
    u8 *ptr = (u8 *)realloc(in, new_size);
    if (ptr == NULL) {
        return false;
    } else {
        *out = ptr;
        return true;
    }
}

void stupid_free(u8 *in) { free(in); }

// MAYBE(yousef): hash table implementation.
// TODO(yousef): public API.
// TODO(yousef): testing.
int main(int argc, char const *argv[]) {
    dbg("sizeof token: %zu bytes", sizeof(token_t));

    char const *filename = argv[1];
    size_t max_file_size = GiB(2);

    size_t input_buffer_size = max_file_size;
    size_t token_buffer_size = max_file_size;
    size_t line_info_buffer_size =
        (token_buffer_size / sizeof(token_t)) * sizeof(size_t);

    size_t pool_size =
        input_buffer_size + token_buffer_size + line_info_buffer_size;

    u8 *reserved_memory_pool;
    if (!alloc_commit(pool_size, &reserved_memory_pool)) {
        panic("unable to allocate required memory at start up");
    }
    u8 *input_buffer = reserved_memory_pool;
    u8 *token_buffer = reserved_memory_pool + input_buffer_size;

    byte_slice file_data = {input_buffer, input_buffer_size};
    read_file(filename, &file_data);

    u8 *line_info_buffer = token_buffer + token_buffer_size;

    agnes_parser_t parser = {0};
    parser.bytes = input_buffer;
    parser.file_size = file_data.len;
    parser.line_info = (size_t *)line_info_buffer;
    parser.filename = filename;
    parser.max_tokens = token_buffer_size / sizeof(token_t);
    parser.tokens = (token_t *)token_buffer;

    allocator_t string_allocator = {0};
    string_allocator.alloc = &stupid_alloc;
    string_allocator.free = &stupid_free;

    parser.string_allocator = string_allocator;

#if defined(_WIN32)
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    int64_t start = counter.QuadPart;
#endif
    agnes_result_t result = parse_json(&parser);

    if (result.kind == RES_PARSER_ERROR || result.kind == RES_LEXER_ERROR) {
        return EXIT_FAILURE;
    }
    dbg("got: %s", format_jvalue(result.jvalue));

#if defined(_WIN32)
    QueryPerformanceCounter(&counter);
    uint64_t end = counter.QuadPart;
    QueryPerformanceFrequency(&counter);
    uint64_t freq = counter.QuadPart;
    dbg("freq: %lld", freq);
    double elapsed = (double)(end - start) / (double)freq;
    dbg("elapsed: %lf", elapsed);
#endif
    return EXIT_SUCCESS;
}