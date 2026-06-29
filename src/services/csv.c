#include <SDL3/SDL.h>
#include <data-structs/dynarr.h>
#include <services/csv.h>
#include <services/servicedefs.h>

// This file is a port of https://github.com/eldskald/csvparse, my other project
// for parsing csv files. I did this to keep not importing any other library
// other than SDL3 to be sure the game is easily portable.

static void store_cell(char** curr_cell, dynarr* curr_row) {
    push_to_dynarr(*curr_cell, curr_row);
    *curr_cell = SDL_calloc(CSV_STARTING_CELL_SIZE, sizeof(char));
}

static void store_row(dynarr** curr_row, dynarr* res) {
    push_to_dynarr(*curr_row, res);
    *curr_row = SDL_malloc(sizeof(dynarr));
    init_dynarr(*curr_row);
}

dynarr CSV_parse(const char* data) {
    dynarr res = (dynarr){0};
    init_dynarr(&res);

    char* curr_cell = SDL_calloc(CSV_STARTING_CELL_SIZE, sizeof(char));
    dynarr* curr_row = SDL_malloc(sizeof(dynarr));
    init_dynarr(curr_row);

    bool curr_cell_starts_with_dquote = false;
    bool just_ended_quot_cell = false;

    char last_char = '\0';

    for (const char* p = data; *p; p++) {
        char curr = *p;

        // Dealing with "
        if (curr == '"' && SDL_strlen(curr_cell) == 0) {
            curr_cell_starts_with_dquote = true;
            last_char = '"';
            continue;
        }
        if (curr == '"' && curr_cell_starts_with_dquote) {
            just_ended_quot_cell = true;
            last_char = '"';
            continue;
        }
        if (just_ended_quot_cell && curr != ',' && curr != '\n') {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "CSV ERROR: cell starts with \" but doesn't end with it");
            return (dynarr){0};
        }

        // Dealing with ,
        if (curr == ',' &&
            !(curr_cell_starts_with_dquote && !just_ended_quot_cell)) {
            store_cell(&curr_cell, curr_row);
            just_ended_quot_cell = false;
            curr_cell_starts_with_dquote = false;
            last_char = ',';
            continue;
        }

        // Dealing with \n
        if (curr == '\n') {

            // Skip empty lines.
            if (last_char == '\n') continue;

            if (last_char != ',') store_cell(&curr_cell, curr_row);
            store_row(&curr_row, &res);
            just_ended_quot_cell = false;
            curr_cell_starts_with_dquote = false;
            last_char = '\n';
            continue;
        }

        // Dealing with every other char
        char tmp[2] = {curr, '\0'};
        SDL_strlcat(curr_cell, tmp, CSV_STARTING_CELL_SIZE);
        last_char = curr;
        continue;
    }

    if (SDL_strlen(curr_cell) > 0) store_cell(&curr_cell, curr_row);
    if (last_char != '\n') store_row(&curr_row, &res);
    SDL_free(curr_cell);
    clear_dynarr(curr_row);
    SDL_free(curr_row);

    return res;
}

void CSV_free(dynarr* data) {
    for (int i = 0; i < data->len; i++) {
        dynarr* row = data->at[i];
        for (int j = 0; j < row->len; j++) {
            SDL_free(row->at[j]);
        }
        clear_dynarr(row);
        SDL_free(row);
    }
    clear_dynarr(data);
}
