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
# This uses the LLVM Exception.
#

.PHONY: all clean help

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Wshadow -Wvla -Wconversion \
    -Wformat-security -fstack-protector-strong -D_FORTIFY_SOURCE=3 \
    -ITrazocore/src
LDFLAGS ?= -lm

SRCDIR := Trazocore/src
BINDIR := bin
BUILDDIR := build

# Core source files
SOURCES := \
    $(SRCDIR)/main.c \
	$(SRCDIR)/lexer.c \
	$(SRCDIR)/preprocessor.c \
	$(SRCDIR)/ffi.c \
	$(SRCDIR)/modules.c \
	$(SRCDIR)/parser.c \
	$(SRCDIR)/typechecker.c \
	$(SRCDIR)/mangle.c \
	$(SRCDIR)/ccodegen.c

OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

# Target executable
TARGET := $(BINDIR)/Trazo

# --- Operating System Detection ---
UNAME_S := $(shell uname -s 2>/dev/null)
OS_NAME := $(UNAME_S)
ifeq ($(OS_NAME),)
    OS_NAME := Windows_NT
endif
ifneq (,$(findstring MINGW,$(OS_NAME)))
    OS_NAME := Windows_NT
endif
ifneq (,$(findstring MSYS,$(OS_NAME)))
    OS_NAME := Windows_NT
endif

ifeq ($(OS_NAME),Windows_NT)
    TARGET := $(BINDIR)/Trazo.exe
    RM := del /Q
else
    RM := rm -f
endif

# --- Rules ---

all: $(TARGET)
	@echo "Trazo compiler built successfully: $(TARGET)"

$(BUILDDIR):
	@mkdir -p $@

$(BINDIR):
	@mkdir -p $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Compilation successful: $(TARGET)"

clean:
	@echo "Cleaning build artifacts..."
ifneq (,$(findstring MINGW,$(UNAME_S)))
	rm -rf $(BUILDDIR) $(BINDIR)
	rm -f *.o *.obj *.exe *.dll *.lib *.so *.dylib *.a *.c *.h
	rm -f examples_ejemplos/*.o examples_ejemplos/*.obj examples_ejemplos/*.exe examples_ejemplos/*.dll examples_ejemplos/*.lib examples_ejemplos/*.so examples_ejemplos/*.dylib examples_ejemplos/*.a examples_ejemplos/*.c examples_ejemplos/*.h
	rm -f tests/*.o tests/*.obj tests/*.exe tests/*.dll tests/*.lib tests/*.so tests/*.dylib tests/*.a tests/*.c tests/*.h
else ifneq (,$(findstring MSYS,$(UNAME_S)))
	rm -rf $(BUILDDIR) $(BINDIR)
	rm -f *.o *.obj *.exe *.dll *.lib *.so *.dylib *.a *.c *.h
	rm -f examples_ejemplos/*.o examples_ejemplos/*.obj examples_ejemplos/*.exe examples_ejemplos/*.dll examples_ejemplos/*.lib examples_ejemplos/*.so examples_ejemplos/*.dylib examples_ejemplos/*.a examples_ejemplos/*.c examples_ejemplos/*.h
	rm -f tests/*.o tests/*.obj tests/*.exe tests/*.dll tests/*.lib tests/*.so tests/*.dylib tests/*.a tests/*.c tests/*.h
else ifeq ($(UNAME_S),)
	@if exist $(BUILDDIR) rmdir /S /Q $(BUILDDIR)
	@if exist $(BINDIR) rmdir /S /Q $(BINDIR)
	@for %%D in (. examples_ejemplos tests) do @( \
		for %%E in (c h o obj exe dll lib so dylib a) do @( \
			if exist "%%D\*.%%E" del /Q "%%D\*.%%E" \
		) \
	)
else
	rm -rf $(BUILDDIR) $(BINDIR)
	rm -f *.o *.obj *.exe *.dll *.lib *.so *.dylib *.a *.c *.h
	rm -f examples_ejemplos/*.o examples_ejemplos/*.obj examples_ejemplos/*.exe examples_ejemplos/*.dll examples_ejemplos/*.lib examples_ejemplos/*.so examples_ejemplos/*.dylib examples_ejemplos/*.a examples_ejemplos/*.c examples_ejemplos/*.h
	rm -f tests/*.o tests/*.obj tests/*.exe tests/*.dll tests/*.lib tests/*.so tests/*.dylib tests/*.a tests/*.c tests/*.h
endif

help:
	@echo "Trazo Compiler - Makefile"
	@echo "Usage:"
	@echo "  make          Build Trazo compiler"
	@echo "  make clean    Remove build artifacts"
	@echo "  make help     Show this message"
