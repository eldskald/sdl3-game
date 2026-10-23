#pragma once

// Load data and allocates initial memory. Call it at the start of the game.
void REACTIONS_start(void);

// Frees memory allocated at `REACTIONS_start()`. Call it when quitting.
void REACTIONS_stop(void);



// Loops through the empty and platform tiles on the simulated area, checking
// their fluid cells for reactions. Call it on the fluids thread, so fluids are
// cleared right away, but it should work on other threads.
void REACTIONS_do_fluid_fluid_to_tile_reactions(void);
