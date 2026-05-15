# Load .env default values and then .env itself
-include .env
export

# File/directory names
APP_NAME	?= game
SRC_FILES	= $(shell find src -type f -name '*.c')

# Compilers
LINUX_CC := gcc
WIN_CC	 := x86_64-w64-mingw32-gcc

# Compile flags
LINUX_COMP_FLAGS	= -std=c99 -Wall -I./src
WIN_COMP_FLAGS		= -std=c99 -Wall -I./src -I./SDL3/include

# Linker flags
LINUX_LINK_FLAGS	= -lSDL3 -lpthread
WIN_LINK_FLAGS		= -L./SDL3/lib -lSDL3

# Setting platform commands
ifeq ($(DEV_PLATFORM), Linux)
	EXT				:= .x86_64
	DEV_CC			:= $(LINUX_CC)
	DEV_COMP_FLAGS	:= $(LINUX_COMP_FLAGS) -DDEV
	DEV_LINK_FLAGS	:= $(LINUX_LINK_FLAGS)
endif
ifeq ($(DEV_PLATFORM), Windows)
	EXT				:= .exe
	DEV_CC			:= $(WIN_CC)
	DEV_COMP_FLAGS	:= $(WIN_COMP_FLAGS) -DDEV
	DEV_LINK_FLAGS	:= $(WIN_LINK_FLAGS)
endif

dev:
	$(DEV_CC) $(SRC_FILES) $(DEV_COMP_FLAGS) $(DEV_LINK_FLAGS) -o $(APP_NAME)$(EXT)

clean:
	rm $(APP_NAME)$(EXT)
