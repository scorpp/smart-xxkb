#include <stdint.h>

#ifndef SMART_LAYOUT_DAEMON_LRU_H
#define SMART_LAYOUT_DAEMON_LRU_H

typedef uint32_t lru_hash_t;
typedef unsigned char lru_value_t;

typedef struct LruEntry {
    lru_hash_t hash;
    lru_value_t value;
    struct LruEntry *next;
} LruEntry;

typedef struct {
    LruEntry *head;
    uint32_t nItems;
    uint32_t capacity;
    uint32_t size;
    int64_t seed;
} LruCache;


void lru_print(LruCache *cache);
char * lru_serialize(LruCache *cache);
LruCache * lru_deserialize(const char *str);
LruCache * lru_new(uint32_t capacity);
void lru_free(LruCache *cache);
void lru_set(LruCache *cache, const char *key, lru_value_t value);
int  lru_get(LruCache *cache, const char *key, lru_value_t *value);

#endif //SMART_LAYOUT_DAEMON_LRU_H
