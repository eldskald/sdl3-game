# WIP

Me messing around with C and SDL3.

## Developing

We are dynamically linking with SDL3 and compiling with MinGW-w64 and gcc. You will need WSL2 if you are on Windows. To compile for Linux, you need nothing other than gcc and SDL3 from your package manager. To compile to Windows, you will need to download SDL3 for MinGWW [here](https://github.com/libsdl-org/SDL/releases), unzip and move `x86_64-w64-mingw32/bin/SDL3.dll` to this project's root, then make an `SDL3` directory and put `x86_64-w64-mingw32/include` and `x86_64-w64-mingw32/lib` in it.

You will also need [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) to compile the shaders. Download the artifact from the repo, unpack it, rename it to `SDL_shadercross` and put it on this project's root in order to run the binaries.

If you want to change or add new fonts, you'll need to use the [bin/ttf2hex](bin/ttf2hex) script we have here. Since this game is pixel art we thought we won't need a robust text drawing library such as SDL_ttf, all we need is a compact pixel font, no antialiasing, scaling or other features needed. So we're rolling our own lightweight and fast solution here. We turn TTF fonts into HEX files that contain a bitmap of the pixel font and use them to make our own atlas texture when the game starts to draw them from. So to add a new font, use the script to get the HEX file, see more at `src/engine/text.c`. The script depends on [ttx](https://fonttools.readthedocs.io/en/latest/ttx.html), [xmlstarlet](https://xmlstar.sourceforge.net/) and [gawk](https://www.gnu.org/software/gawk/), all of which should be available at your Linux distro.

Lastly, for the level editor we are using [Tiled](https://www.mapeditor.org/). At root we have the [editor.tiled-project](editor.tiled-project) with all the setup for editing and making new chunks.

After everything is setup, run a dev build with `bin/dev`, which also compiles the shaders. You can also run `bin/test` to run tests.

You can lint with `bin/lint` and format with `bin/format` if you have clang-tidy and clang-format.
