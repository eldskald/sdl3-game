#include "dev.h"
#include "../data-structs/hashmap.h"
#include "services/json.h"
#include <SDL3/SDL.h>

void DEV_start(void) {
    // int test1 = 42;
    // int test2 = 7;
    // hashmap m = (hashmap){0};
    // init_hashmap(&m);
    // set_on_hashmap(&test1, "chavea", &m);
    // set_on_hashmap(&test2, "chaveb", &m);
    // SDL_Log("%d %d", m.indexes.head->val, m.indexes.head->next->val);
    // SDL_Log("%s, %s, %d, %d", m.keys[7], m.keys[58], *(int*)m.values[7],
    // *(int*)m.values[58]);

    jsondata* val = JSON_parse("{\"chavea\":43,\"chaveb\":82,\"chavec\":17}");
    SDL_Log("%lu, %lu", val->obj.len, val->obj.cap);
    SDL_Log("%f, %f, %f",
            ((jsondata*)val->obj.values[7])->num,
            ((jsondata*)val->obj.values[58])->num,
            ((jsondata*)val->obj.values[45])->num);
    SDL_Log(
        "%s, %s, %s", val->obj.keys[7], val->obj.keys[58], val->obj.keys[45]);
    // SDL_Log("%p, %p", val->obj.keys[7], val->obj.keys[58]);
    // SDL_Log("%f", ((jsondata*)get_from_hashmap("chavea", &val->obj))->num);
    // SDL_Log("%f", ((jsondata*)get_from_hashmap("chaveb", &val->obj))->num);

    JSON_free(val);
}

void DEV_update(void) {
}

void DEV_stop(void) {
}
