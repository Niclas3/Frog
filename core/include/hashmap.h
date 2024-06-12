#ifndef __LIB_HASHMAP
#define __LIB_HASHMAP

#include <ostype.h>

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

hashmap_t *hashmap_init(int_32 size);
void hashmap_free(hashmap_t *map);

void *hashmap_set(hashmap_t *map, void *key, void *value);
int hashmap_remove(hashmap_t *map, const void *key);
void *hashmap_get(hashmap_t *map, const void *key);
void *hashmap_update(hashmap_t *map, const void *key, void *value);
bool hashmap_has(hashmap_t *map, const void *key);
bool hashmap_is_empty(hashmap_t *map);

#endif
