agnes
===
Simple JSON parser with a custom hashtable for string interning to be used in C/C++ applications.

# How to Use?
1. Put the three headers "common.h", "interner.h", and "parser.h" somewhere in your codebase.
2. Define `AG_PARSER_IMPLEMENT` in exactly one place, then include "parser.h" after that definition.
3. Initialise an `agnes_parser_t` structure correctly (see below).
4. Call `parse_json` with a pointer to the aforementioned struct.
5. Examine the return value.

## `agnes_parser_t`
```C
typedef struct agnes_parser {
    char const *filename; // useful for debugging
    u8 const *bytes; 
    size_t file_size;

    token_t *tokens;
    size_t max_tokens;

    size_t *line_info;

    allocator_t string_allocator;
} agnes_parser_t;
```
You **must** allocate the following buffers:
1. `u8 *bytes`: refers to the raw bytes to be parsed as json, could come from a file, or a conventional string. If you want to parse a file, you must read it yourself, put its conent in such a buffer. `file_size` is the size of this buffer in bytes.
2. `token_t *tokens`: this is used by the parser to store tokens. `max_token` refers to the maximum number of tokens this buffer accepts.
3. `size_t *line_info`: this is used by the parser to store line information for debugging. Its size must be at least `max_tokens * 8` bytes.

For `string_allocator`, you must pass two function pointers `alloc` and `free` which the string interner uses to store its data.
These can be as simple as wrappers around `malloc` and friends, or something more sophisticated. The only requirement is that `alloc` must return `true (1)` when succeeding.

## `example-include-as-header`
For a better understanding of the usage, read the contents of `include_as_head.c` (it is short).
You can build and run it using `run.py`.

# The String Interner
