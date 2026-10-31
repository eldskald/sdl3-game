#pragma once
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "services/defs.h"
#include <SDL3/SDL.h>

// Type of data in a jsondata struct.
typedef enum {
    obj,
    str,
    num,
    arr,
    bln,
} jsontype;

// Struct containing the parsed JSON. Its type can be obj, str, num, arr or bln,
// and only the corresponding prop has data according to its type. Hashmaps and
// dynarrs have jsondata values, get the dynarrs' ones with `JSON_at()` and the
// hashmaps' with `JSON_get()`.
//
// ```
// struct jsondata {
//     jsontype type;
//     union {
//         hashmap obj;
//         dynarr arr;
//         char* str;
//         double num;
//         bool bln;
//     };
// }
// ```
typedef struct {
    jsontype type;
    union {
        hashmap obj;
        dynarr arr;
        char* str;
        double num;
        bool bln;
    };
} jsondata;

// Parses a JSON string, returning a pointer to a jsondata struct. Must be freed
// with JSON_free().
jsondata* JSON_parse(const char* string);

// Frees jsondata pointer returned by JSON_parse(). Always call when you're done
// with the data.
void JSON_free(jsondata* value);

// Element `n` of `arr`, the `arr` of a jsondata.
static inline jsondata* JSON_at(size_t n, const dynarr* arr) {
    return dynarr_at(n, arr);
}

// Value of `key` on `obj`, a jsondata of type `obj`, or `NULL` if missing.
static inline jsondata* JSON_get(const char* key, const jsondata* obj) {
    return get_from_hashmap(key, &obj->obj);
}
