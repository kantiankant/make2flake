#ifndef UTILS_H
#define UTILS_H

#include "make2flake.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* string ops */
char* trim(char *str);
char* strdup_safe(const char *src);
void strlcpy_safe(char *dest, const char *src, size_t size);
void strlcat_safe(char *dest, const char *src, size_t size);

/* token extraction */
bool is_build_token(const char *token);
bool is_compiler_flag(const char *token);
bool is_path_token(const char *token);
void extract_tokens(const char *str, char tokens[][MAX_NAME], int *count);

/* file ops */
bool file_exists(const char *path);
char* read_file(const char *path);

/* String utilities */
bool starts_with(const char *str, const char *prefix);
bool ends_with(const char *str, const char *suffix);
bool contains(const char *haystack, const char *needle);
char* to_lower(const char *str);

/* mem management */
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void xfree(void *ptr);

#endif
