#pragma once

// Tests the real `src/engine/graphics.c` on a GPU device, with no window, on
// the offscreen video driver, unlike every other test, which runs on the stub
// at `tests/stubbed/graphics.c`. Checks both what's recorded during a frame and
// what it draws and dispatches once it ends, reading targets and buffers back.
void test_graphics(void);
