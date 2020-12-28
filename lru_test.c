#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include "lru.h"

int 
main(int argc, char **argv) {
    lru_value_t value;

    LruCache *cache = lru_new(100);
    lru_set(cache, "hello", 1);
    if (0 != lru_get(cache, "hello", &value)) {
        errx(2, "Cannot find value");
    }
    warn("Put 1, fetched %d", value);
    lru_print(cache);

    lru_set(cache, "world", 2);
    lru_print(cache);

    lru_set(cache, "!", 3);
    lru_print(cache);

    lru_set(cache, "qwe", 4);
    lru_print(cache);

    lru_get(cache, "world", &value);
    lru_print(cache);

    lru_set(cache, "qwe", 5);
    lru_print(cache);


    char *string = lru_serialize(cache);
    lru_free(cache);
    cache = lru_deserialize(string);
    free(string);

    printf("deserialized: ");
    lru_print(cache);
    lru_set(cache, "world", 6);
    lru_print(cache);
}
