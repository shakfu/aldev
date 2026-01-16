/**
 * @file lang_dispatch.c
 * @brief Language dispatch system implementation.
 */

#include "lang_dispatch.h"
#include <stdio.h>
#include <string.h>

/* Registered languages */
static const LangDispatchEntry *g_langs[LANG_DISPATCH_MAX_LANGS];
static int g_lang_count = 0;

void lang_dispatch_register(const LangDispatchEntry *entry) {
    if (g_lang_count < LANG_DISPATCH_MAX_LANGS && entry) {
        g_langs[g_lang_count++] = entry;
    }
}

const LangDispatchEntry *lang_dispatch_find_by_command(const char *command) {
    if (!command) return NULL;

    for (int i = 0; i < g_lang_count; i++) {
        const LangDispatchEntry *entry = g_langs[i];
        for (int j = 0; j < entry->command_count; j++) {
            if (strcmp(command, entry->commands[j]) == 0) {
                return entry;
            }
        }
    }
    return NULL;
}

const LangDispatchEntry *lang_dispatch_find_by_extension(const char *path) {
    if (!path) return NULL;

    size_t path_len = strlen(path);

    for (int i = 0; i < g_lang_count; i++) {
        const LangDispatchEntry *entry = g_langs[i];
        for (int j = 0; j < entry->extension_count; j++) {
            const char *ext = entry->extensions[j];
            size_t ext_len = strlen(ext);
            if (path_len >= ext_len &&
                strcmp(path + path_len - ext_len, ext) == 0) {
                return entry;
            }
        }
    }
    return NULL;
}

int lang_dispatch_has_supported_extension(const char *path) {
    return lang_dispatch_find_by_extension(path) != NULL;
}

const LangDispatchEntry **lang_dispatch_get_all(int *count) {
    if (count) *count = g_lang_count;
    return g_langs;
}

void lang_dispatch_print_help(void) {
    if (g_lang_count == 0) {
        printf("  (no languages compiled in)\n");
        return;
    }

    for (int i = 0; i < g_lang_count; i++) {
        const LangDispatchEntry *entry = g_langs[i];
        printf("  %-6s - %s\n",
               entry->commands[0],
               entry->description ? entry->description : entry->display_name);
    }
}
