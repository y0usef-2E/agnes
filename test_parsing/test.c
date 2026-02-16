#define AG_PARSER_IMPLEMENT
#include "common.h"
#include "parser.h"

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

void stupid_free(u8 *in) { free(in); }

// arguments: [1]: filename, [2]: filesize (validated by test.py)
int main(int argc, char const *argv[]) {
    if (argc < 3) {
        panic("insufficient command line arguments");
    }

    char const *filename = argv[1];
    size_t file_size = atoi(argv[2]);

    size_t pool_size = MiB(1000);
    size_t max_file_size = MiB(400);

    if (file_size > max_file_size) {
        panic("input file too big");
    }

    u8 *reserved_memory_pool;

    if (!stupid_alloc(pool_size, &reserved_memory_pool)) {
        panic("unable to allocate required memory at start up");
    }

    FILE *handle = fopen(filename, "r");
    fread(reserved_memory_pool, 1, file_size, handle);

    agnes_parser_t parser = {0};
    parser.bytes = reserved_memory_pool;
    parser.file_size = file_size;
    parser.filename = filename;
    parser.max_tokens = max_file_size / sizeof(token_t);

    parser.tokens = (token_t *)(reserved_memory_pool + max_file_size);
    parser.line_info = (size_t *)(reserved_memory_pool + 2 * max_file_size);

    parser.string_allocator =
        (allocator_t){.alloc = stupid_alloc, .free = stupid_free};

    agnes_result_t result = parse_json(&parser);

    if (result.kind == RES_PARSER_ERROR || result.kind == RES_LEXER_ERROR ||
        result.kind == RES_OUT_OF_SPACE) {
        return EXIT_FAILURE;
    }

    dbg("got: %s", format_jvalue(result.jvalue));

    return EXIT_SUCCESS;
}