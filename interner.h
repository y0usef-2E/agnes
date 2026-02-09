#if !defined(AG_INTERNER_H)
#define AG_INTERNER_H
#include "common.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

/*
Let's try this:
1) For the backing string pool, if a string is allocated,
that allocation remains valid and stable until the whole pool is deallocated.
That is, in case the inital pool is full and a new pool is allocated,
earlier allocations are not moved to this new pool
and pointers to these allocations remain valid.

2) Since pointers to those allocations are returned by one interface,
it is assumed that any pointer within the address space of any of the pools, is
one that represents an exact string previously allocated by the interner, and
not any range of bytes within such a pool.
*/
typedef struct string_interner {
    struct {
        char *key;
        byte_slice value;
    } *string_table;

    u8 *current_pool;
    size_t next_string;
    size_t current_pool_size;

    u8 **pools;
    size_t *pool_sizes;
    size_t pool_at;
    size_t max_pools;

    allocator_t allocator;
} interner_t;

#if defined(AG_INTERNER_IMPLEMENT)
static interner_t interner = {.next_string = UINT64_MAX};

bool in_table(byte_slice slice) {}

#define POOL_ARRAY_SIZE 10u

bool init_global_interner(allocator_t allocator, size_t init_size) {
    u8 *buffer;
    if (allocator.alloc == NULL || !allocator.alloc(init_size, &buffer)) {
        return false;
    }

    u8 *pools;
    if (!allocator.alloc(POOL_ARRAY_SIZE * sizeof(u8 *), &pools)) {
        return false;
    }

    u8 *pool_sizes;
    if (!allocator.alloc(POOL_ARRAY_SIZE * sizeof(size_t), &pool_sizes)) {
        return false;
    }

    interner.max_pools = POOL_ARRAY_SIZE;

    interner.string_table = NULL;

    interner.current_pool = buffer;
    interner.current_pool_size = init_size;
    interner.next_string = 0;

    interner.pools = (u8 **)pools;

    interner.pool_sizes = (u8 **)pool_sizes;

    interner.pool_at = 0;
    interner.pools[interner.pool_at] = interner.current_pool;
    interner.pool_sizes[interner.pool_at] = interner.current_pool_size;

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

    u8 *base = interner.current_pool + interner.next_string;
    size_t temp_next_string = interner.next_string;

    size_t real_length = source.len + 1;
    if (interner.next_string + real_length > interner.current_pool_size) {
        // make new pool
        size_t min = real_length;
        size_t hint = interner.current_pool_size;

        u8 *new_buf;
        size_t buf_size;

        for (size_t k = hint;; k = k / 2) {

            if (k < min) {
                panic("ran out of space while allocating string");
            }

            if (interner.allocator.alloc(k, &new_buf)) {
                buf_size = k;
                break;
            }
        }

        assert(new_buf != NULL);

        if (interner.pool_at + 1 >= interner.max_pools) {
            // realloc pool array
        }

        interner.pool_at += 1;
        interner.pools[interner.pool_at] = new_buf;
        interner.pool_sizes[interner.pool_at] = buf_size;

        interner.current_pool = new_buf;
        interner.current_pool_size = buf_size;
        interner.next_string = 0;

        base = new_buf + 0;
    }

    memcpy(base, source.at, source.len);
    base[real_length - 1] = '\0';
    interner.next_string += real_length;

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