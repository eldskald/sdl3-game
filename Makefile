# Load .env default values and then .env itself
-include .env
export

# File/directory names
GAME_NAME	?= game
SRC_FILES	= $(shell find src -type f -name '*.c')
TEST_SRC_FILES  = $(shell find src -type f -name '*.c' -not -name 'main.c')
TEST_FILES = $(shell find tests -type f -name '*.c')

# Compilers
LINUX_CC := gcc
WIN_CC	 := x86_64-w64-mingw32-gcc

# Compile flags
LINUX_COMP_FLAGS	= -std=c23 -Wall -Isrc --embed-dir=./
WIN_COMP_FLAGS		= -std=c23 -Wall -Isrc -ISDL3/include --embed-dir=./

# Linker flags
LINUX_LINK_FLAGS	= -lSDL3
WIN_LINK_FLAGS		= -L./SDL3/lib -lSDL3

# Setting platform commands
ifeq ($(DEV_PLATFORM), Linux)
	EXT				:= .x86_64
	DEV_CC			:= $(LINUX_CC)
	DEV_COMP_FLAGS	:= $(LINUX_COMP_FLAGS) -g -DDEV
	DEV_LINK_FLAGS	:= $(LINUX_LINK_FLAGS)
endif
ifeq ($(DEV_PLATFORM), Windows)
	EXT				:= .exe
	DEV_CC			:= $(WIN_CC)
	DEV_COMP_FLAGS	:= $(WIN_COMP_FLAGS) -g -DDEV
	DEV_LINK_FLAGS	:= $(WIN_LINK_FLAGS)
endif

.PHONY: all clean dev windows linux

all: windows linux

clean:
	rm -rf windows.zip linux.zip $(GAME_NAME)$(EXT)

dev:
	$(DEV_CC) $(SRC_FILES) $(DEV_COMP_FLAGS) $(DEV_LINK_FLAGS) -o $(GAME_NAME)$(EXT)

test:
	$(DEV_CC) $(TEST_SRC_FILES) $(TEST_FILES) $(DEV_COMP_FLAGS) $(DEV_LINK_FLAGS) -Itests -o test -DTEST -fsanitize=address

windows:
	$(WIN_CC) $(SRC_FILES) $(WIN_COMP_FLAGS) $(WIN_LINK_FLAGS) -o $(GAME_NAME).exe
	zip -r -q windows.zip $(GAME_NAME).exe SDL3.dll res
	rm $(GAME_NAME).exe

linux:
	$(LINUX_CC) $(SRC_FILES) $(LINUX_COMP_FLAGS) $(LINUX_LINK_FLAGS) -o $(GAME_NAME).x86_64
	zip -r -q linux.zip $(GAME_NAME).x86_64 res
	rm $(GAME_NAME).x86_64
