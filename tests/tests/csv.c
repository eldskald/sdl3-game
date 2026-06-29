#include <SDL3/SDL_stdinc.h>
#include <data-structs/dynarr.h>
#include <services/csv.h>
#include <stdio.h>
#include <tests.h>
#include <tests/csv.h>

void test_csv(void) {
    printf(YELLOW "\nTesting CSV parser...\n" RESET);

    dynarr data = CSV_parse("a,b,c\n1,2,3\n");
    expect("Parsing a,b,c\\n1,2,3\\n, checking values...",
           9,
           (bool[]){
               data.len == 2,
               ((dynarr*)data.at[0])->len == 3,
               ((dynarr*)data.at[1])->len == 3,
               SDL_strcmp((char*)((dynarr*)data.at[0])->at[0], "a") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[0])->at[1], "b") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[0])->at[2], "c") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[1])->at[0], "1") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[1])->at[1], "2") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[1])->at[2], "3") == 0,
           });
    CSV_free(&data);

    data = CSV_parse("\"has,comma\",nocomma");
    expect(
        "Parsing \"has,comma\",nocomma, checking values...",
        4,
        (bool[]){
            data.len == 1,
            ((dynarr*)data.at[0])->len == 2,
            SDL_strcmp((char*)((dynarr*)data.at[0])->at[0], "has,comma") == 0,
            SDL_strcmp((char*)((dynarr*)data.at[0])->at[1], "nocomma") == 0,
        });
    CSV_free(&data);

    data = CSV_parse("empty,,cell\n\nempty,line,");
    expect("Parsing empty,,cell\\n\\nempty,line,, checking values...",
           8,
           (bool[]){
               data.len == 2,
               ((dynarr*)data.at[0])->len == 3,
               ((dynarr*)data.at[1])->len == 2,
               SDL_strcmp((char*)((dynarr*)data.at[0])->at[0], "empty") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[0])->at[1], "") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[0])->at[2], "cell") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[1])->at[0], "empty") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[1])->at[1], "line") == 0,
           });
    CSV_free(&data);

    data = CSV_parse("empty\n,\nrow");
    expect("Parsing empty\\n,\\nrow, checking values...",
           7,
           (bool[]){
               data.len == 3,
               ((dynarr*)data.at[0])->len == 1,
               ((dynarr*)data.at[1])->len == 1,
               ((dynarr*)data.at[2])->len == 1,
               SDL_strcmp((char*)((dynarr*)data.at[0])->at[0], "empty") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[1])->at[0], "") == 0,
               SDL_strcmp((char*)((dynarr*)data.at[2])->at[0], "row") == 0,
           });
    CSV_free(&data);

    data = CSV_parse("a,,\"bit,of\"\n,\neverything,,\n\n,\n");
    expect(
        "Parsing a,,\"bit,of\"\\n,\\neverything,,\\n\\n,\\n, checking "
        "values...",
        12,
        (bool[]){
            data.len == 4,
            ((dynarr*)data.at[0])->len == 3,
            ((dynarr*)data.at[1])->len == 1,
            ((dynarr*)data.at[2])->len == 2,
            ((dynarr*)data.at[3])->len == 1,
            SDL_strcmp((char*)((dynarr*)data.at[0])->at[0], "a") == 0,
            SDL_strcmp((char*)((dynarr*)data.at[0])->at[1], "") == 0,
            SDL_strcmp((char*)((dynarr*)data.at[0])->at[2], "bit,of") == 0,
            SDL_strcmp((char*)((dynarr*)data.at[1])->at[0], "") == 0,
            SDL_strcmp((char*)((dynarr*)data.at[2])->at[0], "everything") == 0,
            SDL_strcmp((char*)((dynarr*)data.at[2])->at[1], "") == 0,
            SDL_strcmp((char*)((dynarr*)data.at[3])->at[0], "") == 0,
        });
    CSV_free(&data);
}
