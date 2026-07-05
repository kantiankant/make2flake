#include "utils.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

char* trim(char *str) {
    if (!str) return NULL;
    
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    if (*str == '\0') return str;
    
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    end[1] = '\0';
    
    return str;
}

char* strdup_safe(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *dest = malloc(len + 1);
    if (dest) {
        memcpy(dest, src, len + 1);
    }
    return dest;
}

void strlcpy_safe(char *dest, const char *src, size_t size) {
    if (!dest || !src || size == 0) return;
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len >= size - 1) ? size - 1 : src_len;
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
}

void strlcat_safe(char *dest, const char *src, size_t size) {
    if (!dest || !src || size == 0) return;
    
    size_t dest_len = strlen(dest);
    if (dest_len >= size - 1) return;
    
    size_t available = size - dest_len - 1;
    size_t src_len = strlen(src);
    size_t copy_len = (src_len >= available) ? available : src_len;
    
    memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';
}

bool is_build_token(const char *token) {
    if (!token) return false;
    if (strlen(token) == 0) return false;
    
    if (token[0] == '-') return false;
    
    if (strchr(token, '$') || strchr(token, '(') || strchr(token, ')') || 
        strchr(token, '{') || strchr(token, '}')) {
        return false;
    }
    
    for (const char *p = token; *p; p++) {
        if (*p == '_' && !isalpha(token[0])) return false;
        if (*p == '/' || *p == '\\') return false;
        if (*p == '=' || *p == ':' || *p == ';') return false;
        if (*p == '*' || *p == '?' || *p == '[' || *p == ']') return false;
    }
    
    const char *skip[] = {
        "cc", "gcc", "clang", "make", "cp", "rm", "mkdir", 
        "install", "sed", "awk", "grep", "find", "xargs",
        "echo", "cat", "true", "false", "test", "rmdir",
        "touch", "chmod", "chown", "ln", "mv", "sort", "uniq",
        "src", "build", "dist", "lib", "bin", "include", "obj",
        "docs", "examples", "tests", "test", "tools", "scripts",
        "config", "data", "var", "tmp", "temp", "backup",
        "README", "LICENSE", "CHANGELOG", "TODO", "HACKING",
        "INSTALL", "COPYING", "AUTHORS", "NEWS", "ChangeLog",
        "wildcard", "patsubst", "foreach", "call", "eval", "shell",
        "SRC_DIR", "OBJ_DIR", "BIN_DIR", "INCLUDE_DIR", "PREFIX",
        "DESTDIR", "CC", "CFLAGS", "LDFLAGS", "CPPFLAGS",
        "CXX", "CXXFLAGS", "AR", "RANLIB", "INSTALL",
        "describe", "git",
        NULL
    };
    
    for (size_t i = 0; skip[i] != NULL; i++) {
        if (strcmp(token, skip[i]) == 0) return false;
    }
    
    bool all_upper = true;
    for (const char *p = token; *p; p++) {
        if (islower(*p)) {
            all_upper = false;
            break;
        }
    }
    if (all_upper && strlen(token) > 1) {
        return false;
    }
    
    const char *extensions[] = {".c", ".h", ".cpp", ".hpp", ".cxx", ".hxx", 
                                ".cc", ".hh", ".o", ".a", ".so", ".lo", 
                                ".d", ".dep", ".mk", ".in", ".am", ".ac",
                                ".sh", ".py", ".pl", ".rb", ".go", ".rs",
                                NULL};
    for (size_t i = 0; extensions[i] != NULL; i++) {
        if (ends_with(token, extensions[i])) return false;
    }
    
    return true;
}

bool is_compiler_flag(const char *token) {
    if (!token || token[0] != '-') return false;
    
    const char *flags[] = {
        "-I", "-L", "-l", "-D", "-U", "-O", "-g", "-W",
        "-Wall", "-Wextra", "-std=", "-f", "-m", "-M"
    };
    
    for (size_t i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
        if (starts_with(token, flags[i])) return true;
    }
    
    return false;
}

bool is_path_token(const char *token) {
    return token && strchr(token, '/') != NULL;
}

void extract_tokens(const char *str, char tokens[][MAX_NAME], int *count) {
    if (!str || !count || !tokens) return;
    
    const char *p = str;
    char token[MAX_NAME];
    *count = 0;
    
    while (*p && *count < MAX_DEPS) {
        if (isspace((unsigned char)*p) || *p == '`' || *p == '\'' || 
            *p == '"' || *p == '(' || *p == ')' || *p == ',') {
            p++;
            continue;
        }
        
        int len = 0;
        while (*p && !isspace((unsigned char)*p) && *p != '`' && *p != '\'' && 
               *p != '"' && *p != '(' && *p != ')' && *p != ',') {
            if (len < MAX_NAME - 1) token[len++] = *p;
            p++;
        }
        token[len] = '\0';
        
        if (len > 0 && is_build_token(token)) {
            strlcpy_safe(tokens[*count], token, MAX_NAME);
            (*count)++;
        }
    }
}

bool file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fclose(f);
    return true;
}

char* read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    size_t bytes_read = fread(content, 1, size, f);
    if (bytes_read != (size_t)size) {
        /* File size changed or read error */
        free(content);
        fclose(f);
        return NULL;
    }
    content[size] = '\0';
    fclose(f);
    
    return content;
}

bool starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

bool contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    return strstr(haystack, needle) != NULL;
}

char* to_lower(const char *str) {
    if (!str) return NULL;
    char *lower = strdup_safe(str);
    if (!lower) return NULL;
    for (char *p = lower; *p; p++) {
        *p = tolower((unsigned char)*p);
    }
    return lower;
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Fatal: out of memory\n");
        exit(1);
    }
    return ptr;
}

void *xcalloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (!ptr) {
        fprintf(stderr, "Fatal: out of memory\n");
        exit(1);
    }
    return ptr;
}

void xfree(void *ptr) {
    if (ptr) free(ptr);
}
