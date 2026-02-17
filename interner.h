#if !defined(AG_INTERNER_H)
#define AG_INTERNER_H
#include "common.h"

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

typedef struct string_set_entry {
    size_t hash;
    byte_slice rawptr;
} set_entry_t;

typedef enum ctrl_byte {
    kEmpty = 0x80,
    kDeleted = 0xFEu,

    // kUsed = 0x0xxx_xxxx
} ctrl_byte_t;

typedef struct string_interner {
    struct {
        set_entry_t *hashset;
        u8 *ctrl_bytes;

        size_t hashset_cap;
        size_t hashset_occ;
    };

    u8 *current_pool;
    size_t next_string;
    size_t current_pool_size;

    u8 **pools;
    size_t *pool_sizes;
    size_t pool_at;
    size_t max_pools;

    allocator_t allocator;

    size_t seed;
} interner_t;

#if defined(AG_INTERNER_IMPLEMENT)

#define POOL_ARRAY_SIZE 10u

#define HASH_SET_ENTRIES 8192

size_t H1(size_t hash) { return hash >> 7; }
ctrl_byte_t H2(size_t hash) { return hash & 0x7F; }

#include <time.h>

/**********************
stb_ds.h - v0.67 - public domain data structures - Sean Barrett 2019
https://github.com/nothings/stb
https://nothings.org/stb_ds

Credit to Sean Barrett for hashing routine.
*/
#define STBDS_SIZE_T_BITS ((sizeof(size_t)) * 8)

#define STBDS_ROTATE_LEFT(val, n)                                              \
    (((val) << (n)) | ((val) >> (STBDS_SIZE_T_BITS - (n))))
#define STBDS_ROTATE_RIGHT(val, n)                                             \
    (((val) >> (n)) | ((val) << (STBDS_SIZE_T_BITS - (n))))

// assumes strings are null-terminated
static size_t stbds_hash_string(char *str, size_t seed) {
    size_t hash = seed;
    while (*str)
        hash = STBDS_ROTATE_LEFT(hash, 9) + (unsigned char)*str++;

    // Thomas Wang 64-to-32 bit mix function, hopefully also works in 32 bits
    hash ^= seed;
    hash = (~hash) + (hash << 18);
    hash ^= hash ^ STBDS_ROTATE_RIGHT(hash, 31);
    hash = hash * 21;
    hash ^= hash ^ STBDS_ROTATE_RIGHT(hash, 11);
    hash += (hash << 6);
    hash ^= STBDS_ROTATE_RIGHT(hash, 22);
    return hash + seed;
}

/*
**********************/

static bool bytes_strict_eq(byte_slice left, byte_slice right) {
    if (left.len != right.len) {
        return false;
    }

    // legal access always (since I append null-terminator to all strings)
    if (left.at[0] != right.at[0]) {
        return false;
    }
    return memcmp(left.at, right.at, left.len - 1) == 0;
}

static void rebuild_table(interner_t *interner) {
    size_t old_cap = interner->hashset_cap;
    set_entry_t *old_hashset = interner->hashset;
    u8 *old_ctrl_bytes = interner->ctrl_bytes;

    size_t new_cap = interner->hashset_cap = interner->hashset_cap * 4;

    u8 *new_ctrl_bytes;
    set_entry_t *new_hashset;

    if (!interner->allocator.alloc(interner->hashset_cap * sizeof(set_entry_t),
                                   &new_hashset)) {
        panic("ran out of space while allocating string hashset");
    }

    if (!interner->allocator.alloc(interner->hashset_cap * sizeof(u8),
                                   &new_ctrl_bytes)) {
        panic("ran out of space while allocating string hashset");
    }

    memset(new_ctrl_bytes, kEmpty, new_cap * sizeof(u8));

    for (size_t old_pos = 0; old_pos < old_cap; ++old_pos) {
        if (old_ctrl_bytes[old_pos] != kEmpty) {
            size_t hash = old_hashset[old_pos].hash;
            size_t new_pos = H1(hash) % new_cap;
            size_t new_lim =
                new_pos == 0 ? (interner->hashset_cap - 1) : (new_pos - 1);

            do {
                if (new_ctrl_bytes[new_pos] == kEmpty) {
                    new_ctrl_bytes[new_pos] = old_ctrl_bytes[old_pos];
                    new_hashset[new_pos] = old_hashset[old_pos];
                    break;
                }

                new_pos = (new_pos + 1) % new_cap;
            } while (new_pos != new_lim);
        }
    }

    interner->ctrl_bytes = new_ctrl_bytes;
    interner->hashset = new_hashset;
    interner->hashset_occ = interner->hashset_occ;

    interner->allocator.free(old_ctrl_bytes);
}

byte_slice find_or_insert(interner_t *interner, byte_slice allocated,
                          size_t revert) {

    size_t hash = stbds_hash_string(allocated.at, interner->seed);
    size_t pos = H1(hash) % interner->hashset_cap;
    size_t start = pos;

    do {
        byte_slice candidate = interner->hashset[pos].rawptr;

        bool cmp_res;

        if (H2(hash) == interner->ctrl_bytes[pos] &&
            hash == interner->hashset[pos].hash &&
            (cmp_res = bytes_strict_eq(candidate, allocated))) {
            // case 1, found:
            interner->next_string = revert; // string already stored:
                                            // discard temp allocation
            return candidate;
        }

        if (interner->ctrl_bytes[pos] == kEmpty) {
            interner->ctrl_bytes[pos] = H2(hash);
            interner->hashset[pos] =
                (set_entry_t){.hash = hash, .rawptr = allocated};
            interner->hashset_occ += 1;

            return allocated;
        };

        pos = (pos + 1) % interner->hashset_cap;
        if (pos == start) {
            break;
        }
    } while (true);

    panic("unreachable codepath");
}

byte_slice intern_string(interner_t *interner, byte_slice source) {
    if (interner->next_string == UINT64_MAX) {
        panic("global string interner uninitialised");
    }
    u8 *pool;
    size_t pool_size;

    for (int i = 0; i <= interner->pool_at; ++i) {
        pool = interner->pools[i];
        pool_size = interner->pool_sizes[i];

        if (source.at >= pool && source.at < pool + pool_size) {
            return source;
        }
    }
    u8 *base = interner->current_pool + interner->next_string;
    size_t temp_next_string = interner->next_string;

    size_t real_length = source.len + 1;
    if (interner->next_string + real_length > interner->current_pool_size) {
        // make new pool
        size_t min = real_length;
        size_t hint = interner->current_pool_size * 2;

        u8 *new_buf;
        size_t buf_size;

        for (size_t k = hint;; k = k / 2) {

            if (k < min) {
                panic("ran out of space while allocating string pool");
            }

            if (interner->allocator.alloc(k, &new_buf)) {
                buf_size = k;
                break;
            }
        }

        assert(new_buf != NULL);

        if (interner->pool_at + 1 >= interner->max_pools) {
            u8 *new_pools;
            u8 *new_pool_sizes;

            if (!interner->allocator.alloc(
                    interner->max_pools * 2 * sizeof(u8 *), &new_pools)) {
                panic("out of space while allocating array for pool lookup");
            }

            if (!interner->allocator.alloc(interner->max_pools * 2 *
                                               sizeof(size_t),
                                           &new_pool_sizes)) {
                panic("out of space while allocating array for pool lookup");
            }

            memcpy(new_pools, interner->pools,
                   interner->max_pools * sizeof(u8 *));

            memcpy(new_pool_sizes, interner->pool_sizes,
                   interner->max_pools * sizeof(size_t));

            interner->allocator.free(interner->pools);
            interner->allocator.free(interner->pool_sizes);

            interner->pools = new_pools;
            interner->pool_sizes = new_pool_sizes;
        }

        interner->pool_at += 1;
        interner->pools[interner->pool_at] = new_buf;
        interner->pool_sizes[interner->pool_at] = buf_size;

        interner->current_pool = new_buf;
        interner->current_pool_size = buf_size;
        interner->next_string = 0;

        base = new_buf + 0;
    }

    memcpy(base, source.at, source.len);
    base[real_length - 1] = '\0';
    interner->next_string += real_length;

    byte_slice allocated = {base, real_length};

    double upper_bound = ((double)interner->hashset_cap) * 0.75;
    if ((double)(interner->hashset_occ + 1) > upper_bound) {
        rebuild_table(interner);
    }

    return find_or_insert(interner, allocated, temp_next_string);
}

bool init_global_interner(interner_t *interner, allocator_t allocator,
                          size_t init_string_pool_size) {
    interner->next_string = UINT64_MAX;

    u8 *buffer;
    if (allocator.alloc == NULL ||
        !allocator.alloc(init_string_pool_size, &buffer)) {
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

    interner->max_pools = POOL_ARRAY_SIZE;

    interner->current_pool = buffer;
    interner->current_pool_size = init_string_pool_size;

    interner->pools = (u8 **)pools;

    interner->pool_sizes = (u8 **)pool_sizes;

    interner->pool_at = 0;
    interner->pools[interner->pool_at] = interner->current_pool;
    interner->pool_sizes[interner->pool_at] = interner->current_pool_size;

    interner->allocator = allocator;
    time_t temp_time;
    time(&temp_time);
    interner->seed = *(size_t *)&temp_time;

    set_entry_t *string_set;
    u8 *ctrl_bytes;

    interner->hashset_cap = HASH_SET_ENTRIES;

    size_t buffer_size = HASH_SET_ENTRIES * (sizeof(set_entry_t) + sizeof(u8));

    if (!allocator.alloc(buffer_size, &ctrl_bytes)) { // same allocation
        return false;
    }

    memset(ctrl_bytes, kEmpty, interner->hashset_cap * sizeof(u8));

    string_set = ctrl_bytes + interner->hashset_cap * sizeof(u8);

    interner->hashset = string_set;
    interner->ctrl_bytes = ctrl_bytes;

    // success
    interner->next_string = 0;
    return true;
}

void free_and_invalidate(interner_t *interner) {
    if (interner->next_string == UINT64_MAX) {
        panic("interner uninitialised");
    }

    interner->next_string = UINT64_MAX;
    for (int i = 0; i < interner->pool_at; ++i) {
        u8 *pool = interner->pools[i];
        interner->allocator.free(pool);
    }

    interner->allocator.free(interner->ctrl_bytes); // frees hashset, too
}

bool str_eq(byte_slice left, byte_slice right) { return left.at == right.at; }

#endif
#endif