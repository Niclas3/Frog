#ifndef __LIB_HASHMAP
#define __LIB_HASHMAP

#ifndef __COMPOSITOR_C__
#warning "hashmap.h is internal to compositor. Refactor before using elsewhere!"
#endif


#include <frog/types.h>

typedef uint_32 (*hash_func_t)(const void *, uint_32);
typedef bool (*hash_compare_t)(const void *source, void *target);

struct hashmap_entry {
    char *key;
    void *value;
    struct hashmap_entry *next;  // for separate chaining
};

typedef struct hashmap {
    hash_func_t hash_func;
    hash_compare_t hash_compare;
    int_32 size;
    struct hashmap_entry **entry;  // hashmap
} hashmap_t;

__attribute__((deprecated("Do not use hashmap_init() outside compositor")))
hashmap_t *hashmap_init(int_32 size);
void hashmap_free(hashmap_t *map);

void *hashmap_set(hashmap_t *map, void *key, void *value);
int hashmap_remove(hashmap_t *map, const void *key);
void *hashmap_get(hashmap_t *map, const void *key);
void *hashmap_update(hashmap_t *map, const void *key, void *value);
bool hashmap_has(hashmap_t *map, const void *key);
bool hashmap_is_empty(hashmap_t *map);

#endif
