# Detecting platform
ifeq ($(OS), Windows_NT)
	DEV_PLATFORM := Windows
else ifneq ($(shell uname -r | grep -i microsoft),)
	DEV_PLATFORM := Windows
else
	DEV_PLATFORM := Linux
endif

# File/directory names
GAME_NAME	?= game
SRC_FILES	= $(shell find src -type f -name '*.c')
TEST_STUBBED_FILES = $(shell find tests/stubbed -type f -name '*.c')
TEST_EXCLUDED_FILES = main.c $(notdir $(TEST_STUBBED_FILES))
TEST_SRC_FILES  = $(shell find src -type f -name '*.c' $(foreach f,$(TEST_EXCLUDED_FILES),-not -name '$(f)'))
TEST_FILES = $(shell find tests -type f -name '*.c' -not -path 'tests/stubbed/*')

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
	DEV_COMP_FLAGS	:= $(LINUX_COMP_FLAGS) -g -Og -DDEV
	DEV_LINK_FLAGS	:= $(LINUX_LINK_FLAGS)
endif
ifeq ($(DEV_PLATFORM), Windows)
	EXT				:= .exe
	DEV_CC			:= $(WIN_CC)
	DEV_COMP_FLAGS	:= $(WIN_COMP_FLAGS) -g -Og -DDEV
	DEV_LINK_FLAGS	:= $(WIN_LINK_FLAGS)
endif

.PHONY: all clean dev test windows linux

all: windows linux

clean:
	rm -rf windows.zip linux.zip $(GAME_NAME)$(EXT)

dev:
	$(DEV_CC) $(SRC_FILES) $(DEV_COMP_FLAGS) $(DEV_LINK_FLAGS) -o $(GAME_NAME)$(EXT)

test:
	$(DEV_CC) $(TEST_SRC_FILES) $(TEST_STUBBED_FILES) $(TEST_FILES) -Itests $(DEV_COMP_FLAGS) $(DEV_LINK_FLAGS) -o test -DTEST -fsanitize=address

windows:
	$(WIN_CC) $(SRC_FILES) $(WIN_COMP_FLAGS) -O2 $(WIN_LINK_FLAGS) -o $(GAME_NAME).exe
	zip -r -q windows.zip $(GAME_NAME).exe SDL3.dll
	rm $(GAME_NAME).exe

linux:
	$(LINUX_CC) $(SRC_FILES) $(LINUX_COMP_FLAGS) -O2 $(LINUX_LINK_FLAGS) -o $(GAME_NAME).x86_64
	zip -r -q linux.zip $(GAME_NAME).x86_64
	rm $(GAME_NAME).x86_64
