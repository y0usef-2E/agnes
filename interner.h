#if !defined(AG_INTERNER_H)
#define AG_INTERNER_H
#include "common.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

typedef struct string_interner {
    struct {
        char *key;
        byte_slice value;
    } *string_table;
    u8 *string_stack;
    size_t next_string;
    size_t stack_size;

    allocator_t allocator;
} interner_t;

#if defined(AG_INTERNER_IMPLEMENT)
static interner_t interner = {.next_string = UINT64_MAX};

bool init_global_interner(allocator_t allocator, size_t init_size) {
    u8 *buffer;
    if (allocator.alloc == NULL || !allocator.alloc(init_size, &buffer)) {
        return false;
    }

    interner.string_table = NULL;
    interner.string_stack = buffer;
    interner.stack_size = init_size;
    interner.next_string = 0;

    interner.allocator = allocator;
}

#define STR(src) intern_cstring(src)
#define CSTR(src) (intern_cstring(src).at)

#define SLICE(src, len) ((byte_slice){src, len})
#define INTERN(slice) intern_string(slice)

byte_slice intern_string(byte_slice source) {
    if (interner.next_string == UINT64_MAX) {
        panic("global string interner uninitialised");
    }

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

    if ((slice = shget(interner.string_table, allocated.at)).at != NULL) {
        // string already stored: discard temp allocation
        interner.next_string = temp_next_string;
        return slice;
    } else {
        return shput(interner.string_table, allocated.at, allocated);
    }
}

byte_slice intern_cstring(char const *string) {
    byte_slice slice = (byte_slice){string, strlen(string)};
    return intern_string(slice);
}

bool str_eq(byte_slice left, byte_slice right) { return left.at == right.at; }

#endif

#endif