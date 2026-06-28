#include <SDL3/SDL.h>
#include <data-structs/dynarr.h>
#include <dev/dev.h>
#include <services/csv.h>

void DEV_start(void) {
    dynarr data = CSV_parse("teste1,teste2,teste3\nab,ac,ad");
    SDL_Log("%p, %zu, %zu", data.at, data.cap, data.len);
    SDL_Log("%s, %s, %s\n%s, %s, %s",
            (char*)((dynarr*)data.at[0])->at[0],
            (char*)((dynarr*)data.at[0])->at[1],
            (char*)((dynarr*)data.at[0])->at[2],
            (char*)((dynarr*)data.at[1])->at[0],
            (char*)((dynarr*)data.at[1])->at[1],
            (char*)((dynarr*)data.at[1])->at[2]);
    CSV_free(&data);
}

void DEV_update(void) {
}

void DEV_stop(void) {
}
