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

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Wshadow -Wvla -Wconversion \
    -Wformat-security -fstack-protector-strong \
    -D_FORTIFY_SOURCE=3

.PHONY: all clean

all: Radar
	@echo "Radar built successfully."

Radar: lexer.c parser.c validator.c build_plan.c main.c radar.h
	$(CC) $(CFLAGS) lexer.c parser.c validator.c build_plan.c main.c -o $@

clean:
	rm -f Radar Radar.exe radar-parse radar-parse.exe *.o