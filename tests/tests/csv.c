#include "services/csv.h"
#include "data-structs/dynarr.h"
#include "tests.h"
#include "tests/csv.h"
#include <SDL3/SDL.h>
#include <stdio.h>

void test_csv(void) {
    printf(YELLOW "\nTesting CSV parser...\n" RESET);

    dynarr data = CSV_parse("a,b,c\n1,2,3\n");
    expect("Parsing a,b,c\\n1,2,3\\n, checking values...",
           9,
           (bool[]){
               data.len == 2,
               CSV_row(0, &data)->len == 3,
               CSV_row(1, &data)->len == 3,
               SDL_strcmp(CSV_cell(0, CSV_row(0, &data)), "a") == 0,
               SDL_strcmp(CSV_cell(1, CSV_row(0, &data)), "b") == 0,
               SDL_strcmp(CSV_cell(2, CSV_row(0, &data)), "c") == 0,
               SDL_strcmp(CSV_cell(0, CSV_row(1, &data)), "1") == 0,
               SDL_strcmp(CSV_cell(1, CSV_row(1, &data)), "2") == 0,
               SDL_strcmp(CSV_cell(2, CSV_row(1, &data)), "3") == 0,
           });
    CSV_free(&data);

    data = CSV_parse("\"has,comma\",nocomma");
    expect("Parsing \"has,comma\",nocomma, checking values...",
           4,
           (bool[]){
               data.len == 1,
               CSV_row(0, &data)->len == 2,
               SDL_strcmp(CSV_cell(0, CSV_row(0, &data)), "has,comma") == 0,
               SDL_strcmp(CSV_cell(1, CSV_row(0, &data)), "nocomma") == 0,
           });
    CSV_free(&data);

    data = CSV_parse("empty,,cell\n\nempty,line,");
    expect("Parsing empty,,cell\\n\\nempty,line,, checking values...",
           8,
           (bool[]){
               data.len == 2,
               CSV_row(0, &data)->len == 3,
               CSV_row(1, &data)->len == 2,
               SDL_strcmp(CSV_cell(0, CSV_row(0, &data)), "empty") == 0,
               SDL_strcmp(CSV_cell(1, CSV_row(0, &data)), "") == 0,
               SDL_strcmp(CSV_cell(2, CSV_row(0, &data)), "cell") == 0,
               SDL_strcmp(CSV_cell(0, CSV_row(1, &data)), "empty") == 0,
               SDL_strcmp(CSV_cell(1, CSV_row(1, &data)), "line") == 0,
           });
    CSV_free(&data);

    data = CSV_parse("empty\n,\nrow");
    expect("Parsing empty\\n,\\nrow, checking values...",
           5,
           (bool[]){
               data.len == 2,
               CSV_row(0, &data)->len == 1,
               CSV_row(1, &data)->len == 1,
               SDL_strcmp(CSV_cell(0, CSV_row(0, &data)), "empty") == 0,
               SDL_strcmp(CSV_cell(0, CSV_row(1, &data)), "row") == 0,
           });
    CSV_free(&data);

    data = CSV_parse("a,,\"bit,of\"\n,\neverything,,\n\n,\n");
    expect("Parsing a,,\"bit,of\"\\n,\\neverything,,\\n\\n,\\n, checking "
           "values...",
           7,
           (bool[]){
               data.len == 2,
               CSV_row(0, &data)->len == 3,
               CSV_row(1, &data)->len == 1,
               SDL_strcmp(CSV_cell(0, CSV_row(0, &data)), "a") == 0,
               SDL_strcmp(CSV_cell(1, CSV_row(0, &data)), "") == 0,
               SDL_strcmp(CSV_cell(2, CSV_row(0, &data)), "bit,of") == 0,
               SDL_strcmp(CSV_cell(0, CSV_row(1, &data)), "everything") == 0,
           });
    CSV_free(&data);
}
