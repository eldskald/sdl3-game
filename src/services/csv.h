#pragma once
#include <data-structs/dynarr.h>

// Parses a CSV string. The data returned is a dynarr of rows, each row another
// dynarr of cells, each cell a string.
dynarr CSV_parse(const char* data);

// Iterates through each row freeing the cell strings, then clears the row and
// finally clears the data. Call after you're done with the CSV.
void CSV_free(dynarr* data);
