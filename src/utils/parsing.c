#include "utils/parsing.h"
#include "data-structs/dynarr.h"
#include "data-structs/hashmap.h"
#include "engine/fluids.h"
#include "engine/tilemap.h"
#include "services/csv.h"
#include <SDL3/SDL.h>

static size_t get_size(parsing_type type) {
    switch (type) {
    case parsing_double:
        return sizeof(double);
    case parsing_float:
        return sizeof(float);
    case parsing_int:
        return sizeof(int);
    case parsing_uint8:
        return sizeof(Uint8);
    case parsing_sint8:
        return sizeof(Sint8);
    case parsing_uint32:
        return sizeof(Uint32);
    case parsing_bool:
        return sizeof(bool);
    case parsing_fluid:
        return sizeof(fluid_type);
    case parsing_tile:
        return sizeof(tile_type);
    case parsing_color:
        return sizeof(SDL_FColor);
    }
    return 0;
}

// How many cells a value of `type` takes.
static size_t get_cells(parsing_type type) {
    return type == parsing_color ? 4 : 1;
}

// Writes the value of `name` on `map` into `dst`, then deinits `map`.
static void read_name(hashmap map, const char* name, void* dst) {
    void* val = get_from_hashmap(name, &map);
    if (val)
        SDL_memcpy(dst, val, map.data_size);
    else
        SDL_memset(dst, 0, map.data_size);
    deinit_hashmap(&map);
}

// Reads the value of `type` starting at `cells` into `dst`.
static void read_value(char** cells, parsing_type type, void* dst) {
    switch (type) {
    case parsing_double:
        *(double*)dst = SDL_strtod(cells[0], NULL);
        break;
    case parsing_float:
        *(float*)dst = SDL_strtod(cells[0], NULL);
        break;
    case parsing_int:
        *(int*)dst = SDL_strtol(cells[0], NULL, 10);
        break;
    case parsing_uint8:
        *(Uint8*)dst = SDL_strtol(cells[0], NULL, 10);
        break;
    case parsing_sint8:
        *(Sint8*)dst = SDL_strtol(cells[0], NULL, 10);
        break;
    case parsing_uint32:
        *(Uint32*)dst = SDL_strtoul(cells[0], NULL, 10);
        break;
    case parsing_bool:
        *(bool*)dst = SDL_strtol(cells[0], NULL, 10);
        break;
    case parsing_fluid:
        read_name(FLUIDS_get_string_fluid_types_map(), cells[0], dst);
        break;
    case parsing_tile:
        read_name(TILEMAP_get_string_tile_types_map(), cells[0], dst);
        break;
    case parsing_color:
        *(SDL_FColor*)dst = (SDL_FColor){SDL_strtod(cells[0], NULL) / 255.0f,
                                         SDL_strtod(cells[1], NULL) / 255.0f,
                                         SDL_strtod(cells[2], NULL) / 255.0f,
                                         SDL_strtod(cells[3], NULL) / 255.0f};
        break;
    }
}

static dynarr* get_row(const dynarr* csv, const char* key) {
    for (size_t i = 0; i < csv->len; i++) {
        dynarr* row = CSV_row(i, csv);
        if (SDL_strcmp(CSV_cell(0, row), key) == 0) return row;
    }
    return NULL;
}

// Index of the row naming the columns of a table CSV.
static size_t get_header(const dynarr* csv) {
    size_t header = 0;
    while (CSV_row(header, csv)->len < 2)
        header++;
    return header;
}



void PARSING_read_keys(const dynarr* csv,
                       const parsing_field* fields,
                       size_t n) {
    for (size_t f = 0; f < n; f++) {
        dynarr* row = get_row(csv, fields[f].key);
        if (!row) continue;
        size_t count = fields[f].count ? fields[f].count : 1;
        size_t stride =
            fields[f].stride ? fields[f].stride : get_size(fields[f].type);
        size_t cells = get_cells(fields[f].type);
        Uint8* dst = fields[f].dst;
        for (size_t i = 1 + fields[f].at * cells, k = 0;
             k < count && i + cells <= row->len;
             i += cells, k++, dst += stride)
            read_value(dynarr_at(i, row), fields[f].type, dst);
    }
}

size_t PARSING_count_values(const dynarr* csv, const char* key) {
    dynarr* row = get_row(csv, key);
    return row ? row->len - 1 : 0;
}

size_t PARSING_count_entries(const dynarr* csv) {
    size_t entries = 0;
    for (size_t i = get_header(csv) + 1; i < csv->len; i++)
        if (CSV_row(i, csv)->len >= 2) entries++;
    return entries;
}

size_t PARSING_read_table(const dynarr* csv,
                          const parsing_field* fields,
                          size_t n,
                          size_t entry_size) {
    size_t h = get_header(csv);
    dynarr* header = CSV_row(h, csv);
    hashmap columns = (hashmap){0};
    init_hashmap(&columns, sizeof(size_t));
    for (size_t c = 0; c < header->len; c++)
        set_on_hashmap(&c, CSV_cell(c, header), &columns);
    size_t* cols = SDL_malloc(n * sizeof(size_t));
    for (size_t f = 0; f < n; f++)
        cols[f] = *(size_t*)get_from_hashmap(fields[f].key, &columns);
    deinit_hashmap(&columns);

    size_t entry = 0;
    for (size_t i = h + 1; i < csv->len; i++) {
        dynarr* row = CSV_row(i, csv);
        if (row->len < 2) continue;
        for (size_t f = 0; f < n; f++)
            read_value(dynarr_at(cols[f], row),
                       fields[f].type,
                       (Uint8*)fields[f].dst + entry * entry_size);
        entry++;
    }
    SDL_free(cols);
    return entry;
}
