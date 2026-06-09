#pragma once
#include "servicedefs.h"
#include "../data-structs/dynarr.h"
#include "../data-structs/hashmap.h"
#include <SDL3/SDL.h>

typedef enum {
    obj, str, num, arr,
} jsontype;

typedef struct jsondata jsondata;

struct jsondata {
    jsontype type;
    hashmap obj;
    dynarr arr;
    char* str;
    double num;
};

jsondata* JSON_parse(const char* string);

void JSON_free(jsondata* value);
