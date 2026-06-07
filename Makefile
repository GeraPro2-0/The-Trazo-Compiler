#
# Copyright 2026 GeraPro2_0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Makefile for Trazo
# Supports Windows, Linux, and macOS.

CC ?= cc
CFLAGS ?= -std=c11 -Icore/src -Icore/decNumber/decNumber-icu-368 -O2 -Wall -Wextra
DECNUM_CFLAGS := $(CFLAGS) -Wno-unused-value
LDFLAGS ?= -lm

SRCDIR := core/src
BINDIR := bin
BUILDDIR := build

SOURCES := $(wildcard $(SRCDIR)/*.c)
DECNUMDIR := core/decNumber/decNumber-icu-368
DECNUM_SOURCES := \
    $(DECNUMDIR)/decContext.c \
    $(DECNUMDIR)/decNumber.c \
    $(DECNUMDIR)/decimal32.c \
    $(DECNUMDIR)/decimal64.c \
    $(DECNUMDIR)/decimal128.c

OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
DECNUM_OBJECTS := $(patsubst $(DECNUMDIR)/%.c,$(BUILDDIR)/decNumber/%.o,$(DECNUM_SOURCES))

# --- Operating System Detection ---
OS_NAME := $(shell uname -s 2>/dev/null)
ifeq ($(OS_NAME),)
    OS_NAME := Windows_NT
endif
ifneq (,$(findstring MINGW,$(OS_NAME)))
    OS_NAME := Windows_NT
endif
ifneq (,$(findstring MSYS,$(OS_NAME)))
    OS_NAME := Windows_NT
endif
ifneq (,$(findstring CYGWIN,$(OS_NAME)))
    OS_NAME := Windows_NT
endif

# --- Command configuration according to Shell and OS ---
ifeq ($(OS_NAME),Windows_NT)
    EXEEXT := .exe
    ifneq (,$(findstring sh,$(SHELL)))
        MKDIR_P = mkdir -p "$1"
        RMFILE = rm -f "$1"
        RMDIR = rm -rf "$1"
    else
        # Native CMD.exe
        MKDIR_P = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
        RMFILE = del /Q /F "$(subst /,\,$1)" 2>nul || exit 0
        RMDIR = if exist "$(subst /,\,$1)" rd /s /q "$(subst /,\,$1)"
    endif
else
    EXEEXT :=
    MKDIR_P = mkdir -p "$1"
    RMFILE = rm -f "$1"
    RMDIR = rm -rf "$1"
endif

TARGET := $(BINDIR)/Trazo$(EXEEXT)

# --- Compilation Rules ---
.PHONY: all debug release clean rebuild run

all: $(TARGET)

$(TARGET): $(OBJECTS) $(DECNUM_OBJECTS)
	@echo "Linking target: $(TARGET)..."
	@$(call MKDIR_P,$(BINDIR))
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	@echo "Compiling core: $< -> $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/decNumber/%.o: $(DECNUMDIR)/%.c | $(BUILDDIR)
	@echo "Compiling decNumber: $< -> $@"
	@$(call MKDIR_P,$(BUILDDIR)/decNumber)
	@$(CC) $(DECNUM_CFLAGS) -c $< -o $@

$(BUILDDIR):
	@echo "Creating build directory..."
	@$(call MKDIR_P,$(BUILDDIR))

# --- Configuration Targets ---
debug: CFLAGS := -std=c11 -Icore/src -Icore/decNumber/decNumber-icu-368 -g -O0 -Wall -Wextra -DDEBUG
debug: LDFLAGS := -lm
debug:
	@echo "Configuring build for DEBUG mode..."
	@$(MAKE) rebuild --no-print-directory

release: CFLAGS := -std=c11 -Icore/src -Icore/decNumber/decNumber-icu-368 -O3 -Wall -DNDEBUG
release: LDFLAGS := -lm
release:
	@echo "Configuring build for RELEASE mode..."
	@$(MAKE) rebuild --no-print-directory

rebuild: clean all

clean:
	@echo "Cleaning up build artifacts and binaries..."
	-@$(call RMFILE,$(TARGET))
	-@$(call RMDIR,$(BUILDDIR))
	@echo "Clean finished."

run: all
	@echo "Running $(TARGET)..."
	@./$(TARGET) || true