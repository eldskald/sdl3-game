#pragma once
#include "data-structs/dynarr.h"
#include <SDL3/SDL.h>

// What a CSV value is read as, which is the type it's written as.
//
// ```
// enum parsing_type {
//     parsing_double, // double
//     parsing_float,  // float
//     parsing_int,    // int
//     parsing_uint8,  // Uint8
//     parsing_sint8,  // Sint8
//     parsing_uint32, // Uint32
//     parsing_bool,   // bool, from 0 or 1
//     parsing_fluid,  // fluid_type, from its name
//     parsing_tile,   // tile_type, from its name
//     parsing_color,  // SDL_FColor, from 4 values from 0 to 255
// };
// ```
typedef enum {
    parsing_double,
    parsing_float,
    parsing_int,
    parsing_uint8,
    parsing_sint8,
    parsing_uint32,
    parsing_bool,
    parsing_fluid,
    parsing_tile,
    parsing_color,
} parsing_type;

// A value read from a CSV and written on `dst` as `type`. On key/value CSVs,
// `key` is the first cell of its row and `at` is which of the values after it
// is read, from `0`. Lists read up to `count` values from there, `stride` bytes
// apart on `dst`, where `0` is the size of `type`. On table CSVs, `key` is the
// name of its column, `dst` points to its value on the first entry and `at`,
// `count` and `stride` aren't used.
//
// ```
// struct parsing_field {
//     const char* key;
//     parsing_type type;
//     void* dst;
//     size_t at;
//     size_t count;
//     size_t stride;
// };
// ```
typedef struct {
    const char* key;
    parsing_type type;
    void* dst;
    size_t at;
    size_t count;
    size_t stride;
} parsing_field;

// Reads the `n` `fields` from `csv`, parsed with `CSV_parse()`, whose rows
// start with a key followed by its values. Fields whose keys are missing are
// left as they were.
void PARSING_read_keys(const dynarr* csv,
                       const parsing_field* fields,
                       size_t n);

// How many values there are after `key` on `csv`, parsed with `CSV_parse()`,
// whose rows start with a key followed by its values. `0` if it's missing.
size_t PARSING_count_values(const dynarr* csv, const char* key);

// How many entries there are on `csv`, parsed with `CSV_parse()`, whose first
// row with more than one cell is a header naming its columns and each row with
// more than one cell after it is an entry. Single cell rows are section headers
// organizing the file.
size_t PARSING_count_entries(const dynarr* csv);

// Reads every entry of `csv`, as on `PARSING_count_entries()`, into an array
// of entries of `entry_size` bytes. Each of the `n` `fields` points to its
// value on the first entry, and entry `i` is written `i * entry_size` bytes
// after it. The array must fit `PARSING_count_entries()` entries. Returns how
// many were read.
size_t PARSING_read_table(const dynarr* csv,
                          const parsing_field* fields,
                          size_t n,
                          size_t entry_size);
