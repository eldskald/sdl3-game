#pragma once
#include <SDL3/SDL.h>

typedef struct {
    double x, y;
} flame;



// Creates a new flame with `data` and returns its Id on `flame_id`. Returns `0`
// when everything went right, `2` when data is invalid.
int FLAMES_new_flames(flame data, size_t* flame_id);

// Returns the flame data at `data` at the pool index `flame_id`. Returns `0`
// when everyting went right, `3` if `flame_id` is outside the pool range or `1`
// if there is no flame at this index.
int FLAMES_get_flames(size_t flame_id, flame* data);

// Edits the flame at `flame_id` with `data`. Returns `0` when everyting went
// right, `3` if `flame_id` is outside the pool range or `1` if there is no
// flame at this index.
int FLAMES_edit_flames(size_t flame_id, flame data);

// Deletes the flame at `flame_id`. Returns `0` when everyting went right, `3`
// if `flame_id` is outside the pool range or `1` if there is no flame at this
// index.
int FLAMES_del_flames(size_t flame_id);



// Allocates memory for the flames pool. Call at the start of the game.
void FLAMES_start(void);

// Spawns the particles for each flame. Call at any update thread.
void FLAMES_update(void);

// Frees the allocated memory at `FLAMES_start()`. Call when quitting the game.
void FLAMES_stop(void);
