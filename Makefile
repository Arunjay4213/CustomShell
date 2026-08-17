# cshell - a small POSIX shell
#
# Usage:
#   make            build the release binary (./cshell)
#   make debug      build with -O0 -g3 and assertions enabled
#   make release    same as the default target
#   make test       build, then run the end-to-end test suite
#   make clean      remove build output

CC      ?= cc
BIN     := cshell
BUILD   ?= release

WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
STD      := -std=c11
# POSIX 2008 for sigaction, setenv, strdup and friends; the Darwin macro keeps
# the same declarations visible on macOS, where strict POSIX hides some of them.
DEFINES  := -D_POSIX_C_SOURCE=200809L -D_DARWIN_C_SOURCE

ifeq ($(BUILD),debug)
OPTFLAGS := -O0 -g3
else ifeq ($(BUILD),release)
OPTFLAGS := -O2 -DNDEBUG
else
$(error unknown BUILD "$(BUILD)", expected "debug" or "release")
endif

CFLAGS  := $(STD) $(WARNINGS) $(DEFINES) $(OPTFLAGS) -MMD -MP
LDFLAGS ?=

SRCDIR := src
OBJDIR := build/$(BUILD)
SRCS   := $(wildcard $(SRCDIR)/*.c)
OBJS   := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
DEPS   := $(OBJS:.o=.d)

.PHONY: all release debug test clean

all: $(BIN)

release:
	@$(MAKE) --no-print-directory BUILD=release all

debug:
	@$(MAKE) --no-print-directory BUILD=debug all

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

test: $(BIN)
	tests/run_tests.sh ./$(BIN)

clean:
	rm -rf build $(BIN)

-include $(DEPS)
