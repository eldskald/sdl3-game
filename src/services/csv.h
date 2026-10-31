#pragma once
#include "data-structs/dynarr.h"

// Parses a CSV string. The data returned is a dynarr of rows, each row another
// dynarr of cells, each cell a string. Get them with `CSV_row()` and
// `CSV_cell()`.
dynarr CSV_parse(const char* data);

// Iterates through each row freeing the cell strings, then deinits the row and
// finally deinits the data. Call after you're done with the CSV.
void CSV_free(dynarr* data);

// Row `n` of `csv`, parsed with `CSV_parse()`.
static inline dynarr* CSV_row(size_t n, const dynarr* csv) {
    return dynarr_at(n, csv);
}

// Cell `n` of `row`, from `CSV_row()`.
static inline char* CSV_cell(size_t n, const dynarr* row) {
    return *(char**)dynarr_at(n, row);
}
