#pragma once
#include "../data-structs/dynarr.h"
#include "../data-structs/hashmap.h"
#include "servicedefs.h"
#include <SDL3/SDL.h>

// Type of data in a jsondata struct.
typedef enum {
    obj,
    str,
    num,
    arr,
} jsontype;

// Struct containing the parsed JSON. Its type can be obj, str, num or arr, and
// only the corresponding prop will have data according to its type. Both
// hashmaps and dynarrs have jsondata pointers for their values.
//
// struct jsondata {
//     jsontype type;
//     hashmap obj;
//     dynarr arr;
//     char* str;
//     double num;
// }
typedef struct jsondata jsondata;

struct jsondata {
    jsontype type;
    hashmap obj;
    dynarr arr;
    char* str;
    double num;
};

// Parses a JSON string, returning a pointer to a jsondata struct. Must be freed
// with JSON_free().
jsondata* JSON_parse(const char* string);

// Frees jsondata pointer returned by JSON_parse(). Always call when you're done
// with the data.
void JSON_free(jsondata* value);
