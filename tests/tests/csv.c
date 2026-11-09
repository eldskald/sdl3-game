#include "services/csv.h"
#include "data-structs/dynarr.h"
#include "tests.h"
#include "tests/csv.h"
#include "utils/parsing.h"
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

    // `CSV_parse()` drops the empty cells at the end of the first entry, so
    // they're past the end of its row.
    typedef struct {
        int a;
        int b;
        bool c;
        SDL_FColor col;
    } entry;
    entry entries[2] = {0};
    data = CSV_parse("name,a,b,c,col_r,col_g,col_b,col_a\n"
                     "x,1,,,,,,\n"
                     "y,2,3,1,255,0,0,255\n");
    parsing_field fields[] = {
        {"a", parsing_int, &entries->a},
        {"b", parsing_int, &entries->b},
        {"c", parsing_bool, &entries->c},
        {"col_r", parsing_color, &entries->col},
    };
    size_t n =
        read_csv_table(&data, fields, SDL_arraysize(fields), sizeof(entry));
    expect("Reading a table whose first entry ends in empty cells, checking "
           "values...",
           9,
           (bool[]){
               n == 2,
               entries[0].a == 1,
               entries[0].b == 0,
               !entries[0].c,
               entries[0].col.r == 0.0f && entries[0].col.a == 0.0f,
               entries[1].a == 2,
               entries[1].b == 3,
               entries[1].c,
               entries[1].col.r == 1.0f && entries[1].col.g == 0.0f,
           });
    CSV_free(&data);

    const char* sectioned = "\nk,1,\nl,2,\n,,\n\nh,v\nx,3\n,\nlast";
    dynarr first = CSV_parse_section(sectioned, 0);
    dynarr second = CSV_parse_section(sectioned, 1);
    dynarr third = CSV_parse_section(sectioned, 2);
    dynarr missing = CSV_parse_section(sectioned, 3);
    expect("Parsing each section of a CSV split by blank lines and lines of "
           "empty cells, checking their rows and that past the last one has "
           "none...",
           8,
           (bool[]){
               first.len == 2,
               SDL_strcmp(CSV_cell(1, CSV_row(1, &first)), "2") == 0,
               second.len == 2,
               SDL_strcmp(CSV_cell(0, CSV_row(0, &second)), "h") == 0,
               SDL_strcmp(CSV_cell(1, CSV_row(1, &second)), "3") == 0,
               third.len == 1,
               SDL_strcmp(CSV_cell(0, CSV_row(0, &third)), "last") == 0,
               missing.len == 0,
           });
    CSV_free(&first);
    CSV_free(&second);
    CSV_free(&third);
    CSV_free(&missing);
}
