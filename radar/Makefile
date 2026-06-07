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

# ============================================================================
# Makefile portable para Radar (Soporta Windows, Linux y macOS)
# ============================================================================

CC ?= cc

# -MD genera dinámicamente archivos de dependencias (.d) para cJSON.h y radarparse.c
CFLAGS ?= -std=c11 -I./cJSON -O2 -Wall -Wextra -MD
LDFLAGS ?=

# Detección simplificada y estándar para la extensión de Windows
ifeq ($(OS),Windows_NT)
    EXEEXT := .exe
else
    EXEEXT :=
endif

TARGET := Radar$(EXEEXT)

# Definición directa de los archivos fuente
SOURCES_CORE  := radarparse.c
SOURCES_CJSON := cJSON/cJSON.c

# Generación de objetos planos directamente en el directorio actual
OBJECTS_CORE  := build_radarparse.o
OBJECTS_CJSON := build_cJSON.o
ALL_OBJECTS   := $(OBJECTS_CORE) $(OBJECTS_CJSON)

# Mapeo de archivos de dependencia
DEPS := $(ALL_OBJECTS:.o=.d)

# --- Reglas de Ejecución ---
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(ALL_OBJECTS)
	@echo "Enlazando ejecutable: $(TARGET)..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Compilacion exitosa: $(TARGET) esta listo."

# Regla para compilar el código núcleo de Radar
$(OBJECTS_CORE): radarparse.c
	@echo "Compilando nucleo: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para compilar la biblioteca nativa de cJSON
$(OBJECTS_CJSON): cJSON/cJSON.c
	@echo "Compilando cJSON: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Inclusión segura de dependencias (si existen)
-include $(DEPS)

# El truco definitivo de portabilidad para "clean":
# Al anteponer el símbolo "-", si un comando falla o no es reconocido por 
# el sistema operativo actual, Make ignora el error y pasa inmediatamente 
# a la siguiente línea, asegurando el borrado tanto en Bash como en CMD nativo.
clean:
	@echo "Limpiando el proyecto..."
	-rm -f $(TARGET) $(ALL_OBJECTS) $(DEPS)
	-del /Q /F $(TARGET) $(ALL_OBJECTS) $(DEPS) 2>nul
	@echo "Limpieza finalizada."