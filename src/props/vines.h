#pragma once
#include <SDL3/SDL.h>

// Creates a vine with `segments` segments laid evenly along the path through
// the `len` corners on `path`, pinned at its first corner and, when
// `double_pinned`, also at its last one. Pins must be at the bottom side of a
// solid non-platform tile (the ceiling they're hanging from), if that tile ever
// gets cleared, the pin falls off. Pins without a ceiling fall off right away.
// Once all its pins are loose, the vine eventually fades away. `segments` must
// be greater than zero, `len` at least 2 with a path longer than zero, every
// corner must be inside the tilemap space and pins must be on a tile's bottom
// side, a multiple of the tile height. Returns `2` for invalid data, `0` when
// everything went right.
int VINES_add(const SDL_FPoint* path,
              size_t len,
              Uint8 segments,
              bool double_pinned);



// Allocates initial memory and loads settings. Call at the start of the game.
void VINES_start(void);

// Draws vines. Call on the renderer.
void VINES_draw(SDL_Renderer* renderer);

// Updates vines state. Call on any thread's update function.
void VINES_update(void);

// Frees all alocated memory by `VINES_start()`. Call when quitting.
void VINES_stop(void);



#ifdef TEST
// Returns a copy of the chain of the vine at index `vine_id`, a dynarr of its
// prop point IDs from one end to the other. Free it with `clear_dynarr()`.
#include "data-structs/dynarr.h"
dynarr VINES_test_get_vine(size_t vine_id);
#endif
