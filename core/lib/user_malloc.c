#include <frog/mman.h>
#include <frog/syscall.h>
#include <frog/types.h>

#define USER_ALLOC_PAGE_SIZE       4096U
#define USER_ALLOC_ALIGNMENT       16U
#define USER_ALLOC_MAX_SMALL       1024U
#define USER_ALLOC_CLASS_COUNT     7U
#define USER_ALLOC_MAX_MAP_LENGTH  (16U * 1024U * 1024U)
#define USER_ALLOC_ARENA_MAGIC     0x55415245U
#define USER_ALLOC_BLOCK_MAGIC     0x55424c4bU
#define USER_ALLOC_LARGE_MAGIC     0x554c4152U
#define USER_ALLOC_FREE_MAGIC      0x55465245U

struct user_block {
        uint_32 magic;
        uint_32 mapping_length;
        struct user_block *next;
        uint_32 requested_size;
};

struct user_arena {
        uint_32 magic;
        uint_32 class_size;
        uint_32 block_count;
        uint_32 free_count;
        struct user_arena *next;
        struct user_block *free_list;
        uint_32 class_index;
        uint_32 reserved;
} __attribute__((aligned(USER_ALLOC_ALIGNMENT)));

typedef char user_block_header_must_be_16[
    sizeof(struct user_block) == USER_ALLOC_ALIGNMENT ? 1 : -1];
typedef char user_arena_metadata_must_be_aligned[
    sizeof(struct user_arena) % USER_ALLOC_ALIGNMENT == 0 ? 1 : -1];

static struct user_arena *user_arenas[USER_ALLOC_CLASS_COUNT];

static uint_32 user_small_class(uint_32 size, uint_32 *class_index)
{
        uint_32 class_size = USER_ALLOC_ALIGNMENT;
        uint_32 index = 0;

        while (class_size < size && class_size < USER_ALLOC_MAX_SMALL) {
                class_size <<= 1;
                index++;
        }
        if (size > class_size || index >= USER_ALLOC_CLASS_COUNT)
                return 0;
        *class_index = index;
        return class_size;
}

static struct user_arena *user_arena_create(uint_32 class_size,
                                             uint_32 class_index)
{
        struct user_arena *arena = mmap(
            NULL, USER_ALLOC_PAGE_SIZE, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (arena == MAP_FAILED)
                return NULL;
        uint_32 stride = sizeof(struct user_block) + class_size;
        uint_32 count = (USER_ALLOC_PAGE_SIZE - sizeof(*arena)) / stride;

        arena->magic = USER_ALLOC_ARENA_MAGIC;
        arena->class_size = class_size;
        arena->block_count = count;
        arena->free_count = count;
        arena->free_list = NULL;
        arena->class_index = class_index;
        arena->reserved = 0;
        for (uint_32 index = 0; index < count; index++) {
                struct user_block *block = (struct user_block *)
                    ((uint_8 *) arena + sizeof(*arena) + index * stride);

                block->magic = USER_ALLOC_FREE_MAGIC;
                block->mapping_length = 0;
                block->requested_size = 0;
                block->next = arena->free_list;
                arena->free_list = block;
        }
        arena->next = user_arenas[class_index];
        user_arenas[class_index] = arena;
        return arena;
}

static void *user_small_alloc(uint_32 size)
{
        uint_32 class_index;
        uint_32 class_size = user_small_class(size, &class_index);

        if (class_size == 0)
                return NULL;
        struct user_arena *arena = user_arenas[class_index];
        while (arena != NULL && arena->free_list == NULL)
                arena = arena->next;
        if (arena == NULL)
                arena = user_arena_create(class_size, class_index);
        if (arena == NULL || arena->free_list == NULL)
                return NULL;
        struct user_block *block = arena->free_list;

        arena->free_list = block->next;
        arena->free_count--;
        block->magic = USER_ALLOC_BLOCK_MAGIC;
        block->mapping_length = 0;
        block->next = NULL;
        block->requested_size = size;
        return block + 1;
}

static void *user_large_alloc(uint_32 size)
{
        if (size > 0xffffffffU - sizeof(struct user_block) -
                       (USER_ALLOC_PAGE_SIZE - 1U))
                return NULL;
        uint_32 needed = size + sizeof(struct user_block);
        uint_32 mapping_length =
            (needed + USER_ALLOC_PAGE_SIZE - 1U) &
            ~(USER_ALLOC_PAGE_SIZE - 1U);

        if (mapping_length == 0 ||
            mapping_length > USER_ALLOC_MAX_MAP_LENGTH)
                return NULL;
        struct user_block *block = mmap(
            NULL, mapping_length, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (block == MAP_FAILED)
                return NULL;
        block->magic = USER_ALLOC_LARGE_MAGIC;
        block->mapping_length = mapping_length;
        block->next = NULL;
        block->requested_size = size;
        return block + 1;
}

void *malloc(uint_32 size)
{
        if (size == 0)
                return NULL;
        if (size <= USER_ALLOC_MAX_SMALL)
                return user_small_alloc(size);
        return user_large_alloc(size);
}

static void user_arena_unlink(struct user_arena *arena)
{
        struct user_arena **link = &user_arenas[arena->class_index];

        while (*link != NULL && *link != arena)
                link = &(*link)->next;
        if (*link == arena)
                *link = arena->next;
}

void free(void *ptr)
{
        if (ptr == NULL)
                return;
        struct user_block *block = (struct user_block *) ptr - 1;

        if (block->magic == USER_ALLOC_LARGE_MAGIC) {
                uint_32 length = block->mapping_length;

                block->magic = USER_ALLOC_FREE_MAGIC;
                (void) munmap(block, length);
                return;
        }
        if (block->magic != USER_ALLOC_BLOCK_MAGIC)
                return;
        struct user_arena *arena = (struct user_arena *)
            ((uint_32) block & ~(USER_ALLOC_PAGE_SIZE - 1U));

        if (arena->magic != USER_ALLOC_ARENA_MAGIC ||
            arena->class_index >= USER_ALLOC_CLASS_COUNT)
                return;
        block->magic = USER_ALLOC_FREE_MAGIC;
        block->requested_size = 0;
        block->next = arena->free_list;
        arena->free_list = block;
        arena->free_count++;
        if (arena->free_count == arena->block_count) {
                user_arena_unlink(arena);
                arena->magic = 0;
                (void) munmap(arena, USER_ALLOC_PAGE_SIZE);
        }
}
