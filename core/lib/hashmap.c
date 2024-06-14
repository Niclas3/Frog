#include <hashmap.h>
#include <string.h>
#include <syscall.h>
uint_32 hash_function(const void *key, uint_32 size);
bool hash_compare(const void *source, void *target);
bool hash_string_compare(const void *source, void *target);
bool hash_int_compare(const void *source, void *target);

// k : u32
// v : address u32
uint_32 hash_function(const void *key, uint_32 size)
{
    uint_32 _key = (uint_32) key;
    _key = ((_key >> 16) ^ _key) * 0x45d9f3b;
    _key = ((_key >> 16) ^ _key) * 0x45d9f3b;
    _key = (_key >> 16) ^ _key;
    return (uint_32) key % size;
}

// Compare key source and target key
bool hash_string_compare(const void *source, void *target)
{
    return !strcmp(source, target);
}

bool hash_int_compare(const void *source, void *target)
{
    return (int) source == (int) target;
}

hashmap_t *hashmap_init(int_32 size)
{
    hashmap_t *map = malloc(sizeof(hashmap_t));
    if (!map) {
        return NULL;
    }
    map->size = size;
    map->hash_func = hash_function;
    map->hash_compare = hash_int_compare;
    map->entry = malloc(size * sizeof(struct hashmap_entry *));
    memset(map->entry, 0, size * (sizeof(struct hashmap_entry *)));
    return map;
}

void hashmap_free(hashmap_t *map)
{
    if (map) {
        for (int i = 0; i < map->size; i++) {
            struct hashmap_entry *target = map->entry[i];
            while (target) {
                struct hashmap_entry *next = target->next;
                // clear this target
                free(target);
                target = next;
            }
        }
        free(map->entry);
        free(map);
    }
}

void *hashmap_set(hashmap_t *map, void *key, void *value)
{
    int_32 index = map->hash_func(key, map->size);
    struct hashmap_entry *entry = map->entry[index];
    if (entry == NULL) {
        entry = malloc(sizeof(struct hashmap_entry));
        if (entry == NULL) {
            return NULL;
        }
        entry->key = key;
        entry->value = value;
        entry->next = NULL;
        map->entry[index] = entry;
    } else {
        struct hashmap_entry *collision_entry =
            malloc(sizeof(struct hashmap_entry));
        collision_entry->value = value;
        collision_entry->key = key;
        collision_entry->next = NULL;
        while (entry->next)
            entry = entry->next;
        entry->next = collision_entry;
    }
    return entry;
}

bool hashmap_has(hashmap_t *map, const void *key)
{
    int_32 index = map->hash_func(key, map->size);
    struct hashmap_entry *entry = map->entry[index];
    if (entry != NULL) {
        while (entry != NULL) {
            if (map->hash_compare(key, entry->key)) {
                return true;
            }
            entry = entry->next;
        }
    }
    return false;
}
bool hashmap_is_empty(hashmap_t *map)
{
    for (int i = 0; i < map->size; i++) {
        struct hashmap_entry *entry = map->entry[i];
        if (entry) {
            return false;
        }
    }
    return true;
}

void *hashmap_get(hashmap_t *map, const void *key)
{
    int_32 index = map->hash_func(key, map->size);
    struct hashmap_entry *entry = map->entry[index];
    if (entry != NULL) {
        if (entry->next == NULL) {
            return entry->value;
        }
        while (entry != NULL) {
            if (map->hash_compare(key, entry->key)) {
                return entry->value;
            }
            entry = entry->next;
        }
    }
    return NULL;
}

int hashmap_remove(hashmap_t *map, const void *key)
{
    int_32 index = map->hash_func(key, map->size);
    struct hashmap_entry **head = &map->entry[index];
    while ((*head) != NULL) {
        if (map->hash_compare(key, (*head)->key)) {
            struct hashmap_entry *next = (*head)->next;
            free(*head);
            *head = next;
            return 0;
        }
        head = &(*head)->next;
    }
    return -1;
}

void *hashmap_update(hashmap_t *map, const void *key, void *value)
{
    int_32 index = map->hash_func(key, map->size);
    struct hashmap_entry *entry = map->entry[index];
    while (entry != NULL) {
        if (map->hash_compare(key, entry->key)) {
            entry->value = value;
            return NULL;
        }
        entry = entry->next;
    }
    return NULL;
}
