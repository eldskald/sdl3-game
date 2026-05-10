# WIP

Me messing around with C and SDL3.

## Developing

We are dynamically linking with SDL3 and compiling with MinGW-w64 and gcc. You will need WSL2 if you are on Windows. To compile for Linux, you need nothing other than gcc and SDL3 from your package manager. To compile to Windows, you will need to download SDL3 for MinGWW [here](https://github.com/libsdl-org/SDL/releases), unzip and move `x86_64-w64-mingw32/bin/SDL3.dll`, `x86_64-w64-mingw32/include` and `x86_64-w64-mingw32/lib` to this project's root.

You also need to setup your `.env` file. Just do `cp .env.sample .env` and change its values if necessary.

After everything is setup, run a dev build with `bin/dev`.

You can lint with `bin/lint` and format with `bin/format` if you have clang-tidy and clang-format.
