/*
 * Copyright 2026 GeraPro2_0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

static const char* get_string_field(const cJSON* object, const char* name, const char* alt_name) {
    cJSON* value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (cJSON_IsString(value) && (value->valuestring != NULL)) {
        return value->valuestring;
    }
    if (alt_name) {
        value = cJSON_GetObjectItemCaseSensitive(object, alt_name);
        if (cJSON_IsString(value) && (value->valuestring != NULL)) {
            return value->valuestring;
        }
    }
    return NULL;
}

static int parse_manifest_object(const cJSON* manifest, size_t index) {
    if (!cJSON_IsObject(manifest)) {
        fprintf(stderr, "Radar parse error: manifest %zu is not an object\n", index);
        return 0;
    }

    const char* radar_version = get_string_field(manifest, "radar", NULL);
    if (!radar_version) {
        fprintf(stderr, "Radar parse error: missing 'radar' field in manifest %zu\n", index);
        return 0;
    }

    cJSON* project = cJSON_GetObjectItemCaseSensitive(manifest, "project");
    if (!cJSON_IsObject(project)) {
        project = cJSON_GetObjectItemCaseSensitive(manifest, "proyecto");
    }
    if (!cJSON_IsObject(project)) {
        fprintf(stderr, "Radar parse error: missing 'project' or 'proyecto' object in manifest %zu\n", index);
        return 0;
    }

    const char* project_name = get_string_field(project, "name", "nombre");
    const char* project_entry = get_string_field(project, "entry", "entrada");
    if (!project_name) {
        fprintf(stderr, "Radar parse error: missing 'project.name' or 'proyecto.nombre' in manifest %zu\n", index);
        return 0;
    }
    if (!project_entry) {
        fprintf(stderr, "Radar parse error: missing 'project.entry' or 'proyecto.entrada' in manifest %zu\n", index);
        return 0;
    }

    printf("Manifest %zu:\n", index);
    printf("  radar: %s\n", radar_version);
    printf("  project name: %s\n", project_name);
    printf("  project entry: %s\n", project_entry);
    return 1;
}

int main(int argc, char* argv[]) {
    const char* path = "Radar.json";
    if (argc >= 2) {
        path = argv[1];
    }

    // Leer el archivo JSON por completo en memoria (cJSON requiere un buffer de texto)
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* data = (char*)malloc(length + 1);
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return 1;
    }
    
    size_t read_bytes = fread(data, 1, length, file);
    data[read_bytes] = '\0';
    fclose(file);

    // Parsear el JSON
    cJSON* root = cJSON_Parse(data);
    free(data); // El buffer ya no es necesario tras el parseo

    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Failed to parse JSON. Error before: %s\n", error_ptr);
        } else {
            fprintf(stderr, "Failed to parse JSON (unknown error).\n");
        }
        return 1;
    }

    int success = 1;
    if (cJSON_IsArray(root)) {
        size_t index = 0;
        cJSON* value = NULL;
        cJSON_ArrayForEach(value, root) {
            if (!parse_manifest_object(value, index)) {
                success = 0;
            }
            index++;
        }
    } else {
        success = parse_manifest_object(root, 0);
    }

    cJSON_Delete(root);
    return success ? 0 : 1;
}