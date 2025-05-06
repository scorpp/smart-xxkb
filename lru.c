#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <err.h>
#include "lru.h"

// size of single entry in serialized string representation
#define LRU_ENTRY_SERIALIZED_SIZE 10

// TODO #define assert_value

/**
 * MurmurHash2 by Austin Appleby
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
 */
uint32_t MurmurHash2(const void *key, int len, uint32_t seed) {
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t h = seed ^len;
    const unsigned char *data = (const unsigned char *) key;

    while (len >= 4) {
        uint32_t k = *(uint32_t *) data;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        len -= 4;
    }

    switch (len) {
        case 3:
            h ^= data[2] << 16;
        case 2:
            h ^= data[1] << 8;
        case 1:
            h ^= data[0];
            h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

__always_inline lru_hash_t
_lru_string_hash(const char *key, time_t seed) {
    return MurmurHash2(key, (int) (strlen(key) + 1), (uint32_t) seed);
}

void
_lru_move_to_head(LruCache *cache, LruEntry *src, LruEntry *prev) {
    if (prev) { // unless it IS head
        prev->next = src->next;
        src->next = cache->head;
        cache->head = src;
    }
}

void
_lru_append_entry(LruCache *cache, lru_hash_t keyHash, lru_value_t value) {
    LruEntry *entry = calloc(1, sizeof(LruEntry));

    entry->hash = keyHash;
    entry->value = value;
    entry->next = cache->head;
    cache->head = entry;
}

void
_lru_set(LruCache *cache, lru_hash_t keyHash, lru_value_t value) {
    LruEntry *entry, *prev = NULL;

    for (entry = cache->head; entry; prev = entry, entry = entry->next) {
        if (entry->hash == keyHash) {
            entry->value = value;
            return _lru_move_to_head(cache, entry, prev);
        }
    }

    _lru_append_entry(cache, keyHash, value);

    if (cache->size < cache->capacity) {
        cache->size++;
    } else {
        prev->next = NULL;
        free(entry);
    }
}

char *
lru_serialize(LruCache *cache) {
    char *str, *tmp;
    LruEntry *entry = cache->head;

    str = tmp = malloc(/* TODO 2UL * 8UL + (LRU_ENTRY_SERIALIZED_SIZE + 1UL) * cache->capacity + 1UL*/2048);

    tmp += sprintf(tmp, "%8x %8lx ", cache->capacity, cache->seed);
    while (entry) {
        tmp += sprintf(tmp, entry->next ? "%8x=%d " : "%8x=%d", entry->hash, entry->value);
        entry = entry->next;
    }

    return str;
}

LruCache *
lru_deserialize(const char *str) {
    uint32_t    capacity;
    uint32_t    size     = 0;
    time_t      seed;
    LruCache    *cache;
    LruEntry    *entry   = NULL;
    const char  *s       = str;

    sscanf(s, "%x %lx ", &capacity, &seed);
    s += 17;
    cache = lru_new(capacity);
    cache->seed = seed;

    while (*s && size < capacity) {
        if (*s == ' ') s += 1;
        if (!*s) break;

        lru_hash_t keyHash = (lru_hash_t) strtoul(s, NULL, 16);
        lru_value_t value = (lru_value_t) (s[LRU_ENTRY_SERIALIZED_SIZE - 1] - '0'); // char '0' to digit 0
        if (0 > value || value > 9) abort();

        s += LRU_ENTRY_SERIALIZED_SIZE;

        if (cache->head) {
            entry->next = calloc(1, sizeof(LruEntry));
            entry = entry->next;
        } else {
            cache->head = entry = calloc(1, sizeof(LruEntry));
        }
        entry->hash = keyHash;
        entry->value = value;
        size++;
    };

    return cache;
}

void
lru_print(LruCache *cache) {
    char *str = lru_serialize(cache);
    printf("%s\n", str);
    free(str);
}

LruCache *
lru_new(uint32_t capacity) {
    LruCache *cache;

    cache = calloc(1, sizeof(LruCache));
    cache->capacity = capacity;
    cache->seed = time(NULL);

    return cache;
}

void
lru_free(LruCache *cache) {
    for (LruEntry *entry = cache->head; entry; entry = cache->head) {
        cache->head = entry->next;
        free(entry);
    }
    free(cache);
}

void
lru_set(LruCache *cache, const char *key, lru_value_t value) {
    lru_hash_t keyHash = _lru_string_hash(key, cache->seed);
    _lru_set(cache, keyHash, value);
}

int
lru_get(LruCache *cache, const char *key, lru_value_t *value) {
    LruEntry *entry;
    LruEntry *prev = NULL;
    lru_hash_t keyHash = _lru_string_hash(key, cache->seed);

    for (entry = cache->head; entry; prev = entry, entry = entry->next) {
        if (entry->hash == keyHash) {
            *value = entry->value;

            // move found entry to head to implement LRU
            _lru_move_to_head(cache, entry, prev);
            return 0;
        }
    }

    return 404;
}
