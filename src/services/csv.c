#include "services/csv.h"
#include "data-structs/dynarr.h"
#include "services/defs.h"
#include <SDL3/SDL.h>

// This file is a port of https://github.com/eldskald/csvparse, my other project
// for parsing CSV files. I did this to keep not importing any other library
// other than SDL3 to be sure the game is easily portable.

static void store_cell(char** curr_cell, dynarr* curr_row) {
    push_to_dynarr(curr_cell, curr_row);
    *curr_cell = SDL_calloc(CSV_STARTING_CELL_SIZE, sizeof(char));
}

static void store_row(dynarr* curr_row, dynarr* res) {
    while (curr_row->len > 0 &&
           SDL_strlen(CSV_cell(curr_row->len - 1, curr_row)) == 0) {
        SDL_free(CSV_cell(curr_row->len - 1, curr_row));
        curr_row->len--;
    }
    if (curr_row->len == 0) return;

    push_to_dynarr(curr_row, res);
    init_dynarr(curr_row, sizeof(char*));
}

dynarr CSV_parse(const char* data) {
    dynarr res = (dynarr){0};
    init_dynarr(&res, sizeof(dynarr));

    char* curr_cell = SDL_calloc(CSV_STARTING_CELL_SIZE, sizeof(char));
    dynarr curr_row = (dynarr){0};
    init_dynarr(&curr_row, sizeof(char*));

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
            store_cell(&curr_cell, &curr_row);
            just_ended_quot_cell = false;
            curr_cell_starts_with_dquote = false;
            last_char = ',';
            continue;
        }

        // Dealing with \n
        if (curr == '\n') {

            // Skip empty lines.
            if (last_char == '\n') continue;

            if (last_char != ',') store_cell(&curr_cell, &curr_row);
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

    if (SDL_strlen(curr_cell) > 0) store_cell(&curr_cell, &curr_row);
    if (last_char != '\n') store_row(&curr_row, &res);
    SDL_free(curr_cell);
    deinit_dynarr(&curr_row);

    return res;
}

// Length of the line at `p`, its line break included, if it's blank, only
// having empty cells, or `0` if it's not or it's the end of `data`.
static size_t blank_line_len(const char* p) {
    const char* q = p;
    while (*q == ',' || *q == '\r')
        q++;
    return *q == '\n' ? (size_t)(q - p + 1) : 0;
}

static const char* skip_blank_lines(const char* p) {
    for (size_t len = blank_line_len(p); len; len = blank_line_len(p))
        p += len;
    return p;
}

// Whether `p` is on a line break followed by a blank line.
static bool is_section_end(const char* p) {
    return p[0] == '\n' && blank_line_len(p + 1);
}

dynarr CSV_parse_section(const char* data, size_t n) {
    const char* start = skip_blank_lines(data);
    const char* end = start;
    while (true) {
        end = start;
        while (*end && !is_section_end(end))
            end++;
        if (n == 0 || !*end) break;
        start = skip_blank_lines(end + 1);
        n--;
    }
    if (n > 0) start = end;
    size_t len = end - start;
    char* section = SDL_malloc(len + 1);
    SDL_memcpy(section, start, len);
    section[len] = '\0';
    dynarr res = CSV_parse(section);
    SDL_free(section);
    return res;
}

void CSV_free(dynarr* data) {
    for (size_t i = 0; i < data->len; i++) {
        dynarr* row = CSV_row(i, data);
        for (size_t j = 0; j < row->len; j++)
            SDL_free(CSV_cell(j, row));
        deinit_dynarr(row);
    }
    deinit_dynarr(data);
}
