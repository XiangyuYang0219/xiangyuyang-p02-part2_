#include <stdint.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

const size_t ALIGNMENT = 2 * sizeof(size_t);
static const size_t OVERHEAD = 2 * sizeof(size_t);

typedef struct block_t block_t;
struct block_t {
    size_t header;
    union {
        struct {
            block_t *next;
            block_t *prev;
        };
        uint8_t payload[0];
    };
};

static const size_t MIN_BLOCK_SIZE = sizeof(block_t) + sizeof(size_t);
static block_t *mm_heap_first;
static block_t *mm_heap_last;
static block_t *free_list_head;

static size_t get_size(const block_t *block) {
    return block->header & ~(size_t)1;
}

static bool is_allocated(const block_t *block) {
    return (block->header & 1) != 0;
}

static size_t *get_footer(block_t *block) {
    return (size_t *)((uint8_t *)block + get_size(block) - sizeof(size_t));
}

static void set_footer(block_t *block) {
    *get_footer(block) = block->header;
}

static void set_header(block_t *block, size_t size, bool allocated) {
    block->header = size | (size_t)allocated;
    set_footer(block);
}

static block_t *get_next_block(block_t *block) {
    if (block == mm_heap_last) {
        return NULL;
    }
    return (block_t *)((uint8_t *)block + get_size(block));
}

static block_t *get_prev_block(block_t *block) {
    if (block == mm_heap_first) {
        return NULL;
    }
    uint8_t *start = (uint8_t *)block;
    size_t footer = *(size_t *)(start - sizeof(size_t));
    return (block_t *)(start - (footer & ~(size_t)1));
}

static block_t *block_from_payload(void *payload) {
    return (block_t *)((uint8_t *)payload - offsetof(block_t, payload));
}

static void list_add(block_t *block) {
    block->prev = NULL;
    block->next = free_list_head;
    if (free_list_head != NULL) {
        free_list_head->prev = block;
    }
    free_list_head = block;
}

static void list_remove(block_t *block) {
    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        free_list_head = block->next;
    }
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }
}

static block_t *find_fit(size_t size) {
    for (block_t *block = free_list_head; block != NULL; block = block->next) {
        if (get_size(block) >= size) {
            return block;
        }
    }
    return NULL;
}

bool mm_init(void) {
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    free_list_head = NULL;
    return mem_sbrk(ALIGNMENT - sizeof(size_t)) != (void *)-1;
}

void *mm_malloc(size_t size) {
    if (size == 0 || size > (size_t)PTRDIFF_MAX - OVERHEAD - (ALIGNMENT - 1)) {
        return NULL;
    }

    size_t needed = (size + OVERHEAD + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
    if (needed < MIN_BLOCK_SIZE) {
        needed = MIN_BLOCK_SIZE;
    }

    block_t *block = find_fit(needed);
    if (block != NULL) {
        size_t available = get_size(block);
        list_remove(block);
        if (available - needed >= MIN_BLOCK_SIZE) {
            block_t *rest = (block_t *)((uint8_t *)block + needed);
            set_header(rest, available - needed, false);
            if (block == mm_heap_last) {
                mm_heap_last = rest;
            }
            list_add(rest);
            available = needed;
        }
        set_header(block, available, true);
        return block->payload;
    }

    block = mem_sbrk((ssize_t)needed);
    if (block == (void *)-1) {
        return NULL;
    }
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;
    set_header(block, needed, true);
    return block->payload;
}

void mm_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    block_t *block = block_from_payload(ptr);
    block_t *prev = get_prev_block(block);
    block_t *next = get_next_block(block);
    size_t size = get_size(block);

    if (next != NULL && !is_allocated(next)) {
        list_remove(next);
        size += get_size(next);
        if (next == mm_heap_last) {
            mm_heap_last = block;
        }
    }
    if (prev != NULL && !is_allocated(prev)) {
        list_remove(prev);
        size += get_size(prev);
        if (block == mm_heap_last) {
            mm_heap_last = prev;
        }
        block = prev;
    }

    set_header(block, size, false);
    list_add(block);
}

void *mm_realloc(void *old_ptr, size_t size) {
    if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    }
    if (old_ptr == NULL) {
        return mm_malloc(size);
    }

    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    size_t copy_size = get_size(block_from_payload(old_ptr)) - OVERHEAD;
    if (size < copy_size) {
        copy_size = size;
    }
    memcpy(new_ptr, old_ptr, copy_size);
    mm_free(old_ptr);
    return new_ptr;
}

void *mm_calloc(size_t nmemb, size_t size) {
    if (size != 0 && nmemb > SIZE_MAX / size) {
        return NULL;
    }
    size_t total = nmemb * size;
    void *ptr = mm_malloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void mm_checkheap(void) {
}
