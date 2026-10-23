#pragma once

// Load data and allocates initial memory. Call it at the start of the game.
void REACTIONS_start(void);

// Same as `REACTIONS_start()`, but loads the game's data instead of the mocks
// used on tests. Only defined on test builds.
void REACTIONS_start_with_non_mocked(void);

// Frees memory allocated at `REACTIONS_start()`. Call it when quitting.
void REACTIONS_stop(void);



// Loops through the empty and platform tiles on the simulated area, checking
// their fluid cells for reactions. Call it on the fluids thread, so fluids are
// cleared right away, but it should work on other threads.
void REACTIONS_do_fluid_fluid_into_tile_reactions(void);
