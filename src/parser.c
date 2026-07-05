#include "parser.h"
#include "utils.h"
#include "dependency.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

typedef struct {
    char name[MAX_NAME];
    char value[MAX_LINE];
} MakeVar;

static MakeVar *variables = NULL;
static int var_count = 0;
static int var_capacity = 0;

static void store_variable(const char *name, const char *value) {
    char clean_name[MAX_NAME];
    strlcpy_safe(clean_name, name, MAX_NAME);
    trim(clean_name);
    
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, clean_name) == 0) {
            strlcat_safe(variables[i].value, " ", MAX_LINE);
            strlcat_safe(variables[i].value, value, MAX_LINE);
            return;
        }
    }
    
    if (var_count >= var_capacity) {
        var_capacity = var_capacity ? var_capacity * 2 : 16;
        variables = realloc(variables, var_capacity * sizeof(MakeVar));
        if (!variables) {
            fprintf(stderr, "Fatal: out of memory\n");
            exit(1);
        }
    }
    
    strlcpy_safe(variables[var_count].name, clean_name, MAX_NAME);
    strlcpy_safe(variables[var_count].value, value, MAX_LINE);
    var_count++;
}

static void expand_variables(const char *src, char *dest, size_t max_len, int depth) {
    if (depth > 20) {
        strlcpy_safe(dest, src, max_len);
        return;
    }
    
    size_t d_idx = 0;
    const char *p = src;
    
    while (*p && d_idx < max_len - 1) {
        if (*p == '$') {
            char close_char = (*(p + 1) == '(') ? ')' : (*(p + 1) == '{' ? '}' : '\0');
            if (close_char && *(p + 1) != '\0') {
                const char *end = strchr(p + 2, close_char);
                if (end) {
                    char var_name[MAX_NAME];
                    size_t len = end - (p + 2);
                    if (len >= MAX_NAME) len = MAX_NAME - 1;
                    strncpy(var_name, p + 2, len);
                    var_name[len] = '\0';
                    
                    bool found = false;
                    for (int i = 0; i < var_count; i++) {
                        if (strcmp(variables[i].name, var_name) == 0) {
                            char expanded[MAX_LINE];
                            expand_variables(variables[i].value, expanded, sizeof(expanded), depth + 1);
                            size_t exp_len = strlen(expanded);
                            if (d_idx + exp_len < max_len - 1) {
                                strcpy(&dest[d_idx], expanded);
                                d_idx += exp_len;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        size_t raw_len = (end + 1) - p;
                        if (d_idx + raw_len < max_len - 1) {
                            strncpy(&dest[d_idx], p, raw_len);
                            d_idx += raw_len;
                        }
                    }
                    p = end + 1;
                    continue;
                }
            }
            if (*(p + 1) != '\0' && isalpha(*(p + 1))) {
                char var_name[2] = {*(p + 1), '\0'};
                bool found = false;
                for (int i = 0; i < var_count; i++) {
                    if (strcmp(variables[i].name, var_name) == 0) {
                        char expanded[MAX_LINE];
                        expand_variables(variables[i].value, expanded, sizeof(expanded), depth + 1);
                        size_t exp_len = strlen(expanded);
                        if (d_idx + exp_len < max_len - 1) {
                            strcpy(&dest[d_idx], expanded);
                            d_idx += exp_len;
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    dest[d_idx++] = *p++;
                    dest[d_idx++] = *p++;
                } else {
                    p += 2;
                }
                continue;
            }
        }
        dest[d_idx++] = *p++;
    }
    dest[d_idx] = '\0';
}

static void detect_build_system(Project *proj) {
    if (file_exists("configure.ac") || file_exists("configure.in")) {
        proj->has_autotools = true;
    }
    if (file_exists("CMakeLists.txt")) {
        proj->has_cmake = true;
    }
    if (file_exists("meson.build")) {
        proj->has_meson = true;
    }
    if (file_exists("setup.py") || file_exists("pyproject.toml")) {
        proj->has_python = true;
    }
}

static void extract_executable_name(const char *src, char *dest, size_t max_len) {
    char expanded[MAX_NAME];
    expand_variables(src, expanded, sizeof(expanded), 0);
    
    const char *p = expanded;
    const char *last_part = expanded;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            last_part = p + 1;
        }
        p++;
    }
    
    char clean[MAX_NAME] = {0};
    int idx = 0;
    p = last_part;
    
    while (*p && idx < (int)max_len - 1) {
        char c = *p;
        if (c == '$' || c == '(' || c == ')' || c == '{' || c == '}' || 
            c == ' ' || c == '\t') {
            p++;
            continue;
        }
        if (isalnum(c) || c == '-' || c == '_' || c == '.') {
            clean[idx++] = c;
        }
        p++;
    }
    clean[idx] = '\0';
    
    const char *suffixes[] = {".exe", ".out", ".bin", ".elf", NULL};
    for (int i = 0; suffixes[i]; i++) {
        char *s = strstr(clean, suffixes[i]);
        if (s) {
            *s = '\0';
            break;
        }
    }
    
    if (strlen(clean) > 0 && strlen(clean) < 64) {
        strlcpy_safe(dest, clean, max_len);
    } else {
        strlcpy_safe(dest, "", max_len);
    }
}

static bool should_ignore_token(const char *token, const Project *proj) {
    if (proj && proj->name[0] && strcmp(token, proj->name) == 0) {
        return true;
    }
    if (proj && proj->main_target[0] && strcmp(token, proj->main_target) == 0) {
        return true;
    }
    
    const char *ignore_tokens[] = {
        "src", "build", "dist", "lib", "bin", "include", "obj",
        "docs", "examples", "tests", "test", "tools", "scripts",
        "config", "data", "var", "tmp", "temp", "backup",
        "README", "LICENSE", "CHANGELOG", "TODO", "HACKING",
        "INSTALL", "COPYING", "AUTHORS", "NEWS", "ChangeLog",
        "wildcard", "patsubst", "foreach", "call", "eval", "shell",
        "SRC_DIR", "OBJ_DIR", "BIN_DIR", "INCLUDE_DIR", 
        "PREFIX", "DESTDIR", "CC", "CFLAGS", "LDFLAGS", "CPPFLAGS",
        "CXX", "CXXFLAGS", "AR", "RANLIB", "INSTALL",
        "describe", "git",
        NULL
    };
    
    for (size_t i = 0; ignore_tokens[i] != NULL; i++) {
        if (strcmp(token, ignore_tokens[i]) == 0) {
            return true;
        }
    }
    
    /* skip tokens that are all uppercase (makefile vars) */
    bool all_upper = true;
    for (const char *p = token; *p; p++) {
        if (islower(*p)) {
            all_upper = false;
            break;
        }
    }
    if (all_upper && strlen(token) > 1) {
        return true;
    }
    
    return false;
}

static void parse_rule_line(const char *line, Project *proj) {
    char *line_copy = strdup_safe(line);
    if (!line_copy) return;
    
    char *colon = strchr(line_copy, ':');
    if (!colon) {
        free(line_copy);
        return;
    }
    
    *colon = '\0';
    char *target = trim(line_copy);
    char *deps = trim(colon + 1);
    
    const char *skip_targets[] = {
        "all", "default", "clean", "uninstall", "install", 
        "distclean", "mostlyclean", "maintainer-clean",
        ".PHONY", ".SUFFIXES", ".DEFAULT", ".PRECIOUS",
        ".DELETE_ON_ERROR", ".INTERMEDIATE", ".SECONDARY"
    };
    
    for (size_t i = 0; i < sizeof(skip_targets)/sizeof(skip_targets[0]); i++) {
        if (strcmp(target, skip_targets[i]) == 0) {
            if (strcmp(target, "install") == 0) {
                strlcpy_safe(proj->install_target, target, MAX_NAME);
            }
            free(line_copy);
            return;
        }
    }
    
    /* set main target if unset */
    if (proj->main_target[0] == '\0') {
        if (!strchr(target, '%') && !strchr(target, '.') && 
            !strchr(target, '/') && !strchr(target, '\\')) {
            char clean_name[MAX_NAME];
            extract_executable_name(target, clean_name, MAX_NAME);
            if (strlen(clean_name) > 0 && strcmp(clean_name, "project") != 0) {
                strlcpy_safe(proj->main_target, clean_name, MAX_NAME);
                if (proj->name[0] == '\0') {
                    strlcpy_safe(proj->name, clean_name, MAX_NAME);
                }
            }
        } else if (strchr(target, '/') || strchr(target, '\\')) {
            char clean_name[MAX_NAME];
            extract_executable_name(target, clean_name, MAX_NAME);
            if (strlen(clean_name) > 0 && strcmp(clean_name, "project") != 0) {
                strlcpy_safe(proj->main_target, clean_name, MAX_NAME);
                if (proj->name[0] == '\0') {
                    strlcpy_safe(proj->name, clean_name, MAX_NAME);
                }
            }
        }
    }
    
    char tokens[MAX_DEPS][MAX_NAME];
    int token_count = 0;
    char expanded_deps[MAX_LINE];
    expand_variables(deps, expanded_deps, sizeof(expanded_deps), 0);
    extract_tokens(expanded_deps, tokens, &token_count);
    
    for (int i = 0; i < token_count; i++) {
        bool is_phony = false;
        for (size_t j = 0; j < sizeof(skip_targets)/sizeof(skip_targets[0]); j++) {
            if (strcmp(tokens[i], skip_targets[j]) == 0) {
                is_phony = true;
                break;
            }
        }
        
        if (!is_phony && !should_ignore_token(tokens[i], proj) && strlen(tokens[i]) > 0) {
            if (!ends_with(tokens[i], ".o") && !ends_with(tokens[i], ".a") &&
                !ends_with(tokens[i], ".so") && !ends_with(tokens[i], ".lo") &&
                !ends_with(tokens[i], ".d") && !ends_with(tokens[i], ".dep") &&
                !ends_with(tokens[i], ".c") && !ends_with(tokens[i], ".h") &&
                !ends_with(tokens[i], ".cpp") && !ends_with(tokens[i], ".hpp") &&
                !ends_with(tokens[i], ".cxx") && !ends_with(tokens[i], ".hxx") &&
                !ends_with(tokens[i], ".cc") && !ends_with(tokens[i], ".hh") &&
                !ends_with(tokens[i], ".mk") && !ends_with(tokens[i], ".in") &&
                !ends_with(tokens[i], ".am") && !ends_with(tokens[i], ".ac")) {
                add_dependency(proj, tokens[i], DEP_BUILD);
            }
        }
    }
    
    free(line_copy);
}

static void extract_version(const char *value, Project *proj) {
    char expanded[MAX_LINE];
    expand_variables(value, expanded, sizeof(expanded), 0);
    
    if (strchr(expanded, '`') != NULL || strstr(expanded, "$(shell") != NULL) {
        char *var_ref = strstr(expanded, "$(_VERSION)");
        if (var_ref) {
            for (int i = 0; i < var_count; i++) {
                if (strcmp(variables[i].name, "_VERSION") == 0) {
                    char ver[MAX_NAME];
                    expand_variables(variables[i].value, ver, sizeof(ver), 0);
                    trim(ver);
                    if (strlen(ver) > 0) {
                        strlcpy_safe(proj->version, ver, MAX_NAME);
                        return;
                    }
                    break;
                }
            }
        }
        
        const char *p = expanded;
        while (*p) {
            if (isdigit(*p)) {
                char version[MAX_NAME];
                int i = 0;
                while (*p && (isdigit(*p) || *p == '.' || *p == '-') && i < MAX_NAME-1) {
                    version[i++] = *p++;
                }
                version[i] = '\0';
                if (strlen(version) > 0 && strchr(version, '.') != NULL) {
                    strlcpy_safe(proj->version, version, MAX_NAME);
                    return;
                }
            }
            p++;
        }
    } else {
        char clean[MAX_NAME];
        extract_executable_name(expanded, clean, MAX_NAME);
        if (strlen(clean) > 0 && strcmp(clean, "project") != 0) {
            strlcpy_safe(proj->version, clean, MAX_NAME);
        }
    }
}

static void parse_line(const char *line, Project *proj) {
    char *trimmed = trim((char*)line);
    if (!trimmed || trimmed[0] == '\0' || trimmed[0] == '#') return;
    
    if (contains(trimmed, "$(guile")) {
        proj->uses_guile = true;
        fprintf(stderr, "Warning: GNU Make Guile integration detected\n");
    }
    
    if (trimmed[0] != '\t') {
        char *equal = strchr(trimmed, '=');
        if (equal) {
            *equal = '\0';
            char *name = trim(trimmed);
            char *value = trim(equal + 1);
            store_variable(name, value);
            
            if (strcmp(name, "PROJECT") == 0 || strcmp(name, "PROJECT_NAME") == 0 ||
                strcmp(name, "PACKAGE") == 0 || strcmp(name, "PACKAGE_NAME") == 0 ||
                strcmp(name, "NAME") == 0 || strcmp(name, "TARGET") == 0) {
                char clean[MAX_NAME];
                extract_executable_name(value, clean, MAX_NAME);
                if (strlen(clean) > 0 && strcmp(clean, "project") != 0) {
                    strlcpy_safe(proj->name, clean, MAX_NAME);
                }
            }
            
            if (strcmp(name, "VERSION") == 0 || strcmp(name, "PACKAGE_VERSION") == 0 ||
                strcmp(name, "_VERSION") == 0) {
                extract_version(value, proj);
            }
            
            char tokens[MAX_DEPS][MAX_NAME];
            int token_count = 0;
            char expanded[MAX_LINE];
            expand_variables(value, expanded, sizeof(expanded), 0);
            extract_tokens(expanded, tokens, &token_count);
            
            for (int i = 0; i < token_count; i++) {
                if (strlen(tokens[i]) > 0 && is_build_token(tokens[i]) && 
                    !should_ignore_token(tokens[i], proj)) {
                    if (tokens[i][0] == '-' && tokens[i][1] == 'l') {
                        add_dependency(proj, tokens[i] + 2, DEP_BUILD);
                    } else {
                        bool is_tool = false;
                        const char *tools[] = {"gcc", "clang", "make", "autoconf", "automake", 
                                               "cmake", "meson", "ninja", "pkg-config", "flex", 
                                               "bison", "python", "perl", "ruby"};
                        for (size_t j = 0; j < sizeof(tools)/sizeof(tools[0]); j++) {
                            if (strcmp(tokens[i], tools[j]) == 0) {
                                is_tool = true;
                                break;
                            }
                        }
                        add_dependency(proj, tokens[i], is_tool ? DEP_NATIVE : DEP_BUILD);
                    }
                }
            }
        } else {
            parse_rule_line(trimmed, proj);
        }
    } else {
        if (contains(trimmed, "pkg-config")) {
            proj->has_pkg_config = true;
            char *pkg = strstr(trimmed, "pkg-config");
            if (pkg) {
                pkg += 11;
                while (*pkg && isspace(*pkg)) pkg++;
                if (*pkg == '-') {
                    while (*pkg && !isspace(*pkg)) pkg++;
                    while (*pkg && isspace(*pkg)) pkg++;
                }
                char module[MAX_NAME];
                int i = 0;
                while (*pkg && !isspace(*pkg) && i < MAX_NAME-1) {
                    module[i++] = *pkg++;
                }
                module[i] = '\0';
                if (strlen(module) > 0 && !should_ignore_token(module, proj)) {
                    add_dependency(proj, module, DEP_BUILD);
                }
            }
            
            if (contains(trimmed, "pixman")) {
                add_dependency(proj, "pixman", DEP_BUILD);
            }
        }
        
        if (contains(trimmed, "gcc") || contains(trimmed, "clang") || 
            contains(trimmed, "cc") || contains(trimmed, "c++") ||
            contains(trimmed, "g++") || contains(trimmed, "clang++")) {
            char *p = (char*)trimmed;
            while ((p = strstr(p, "-l")) != NULL) {
                p += 2;
                char lib[MAX_NAME];
                int i = 0;
                while (*p && !isspace(*p) && i < MAX_NAME-1) {
                    lib[i++] = *p++;
                }
                lib[i] = '\0';
                if (strlen(lib) > 0 && !should_ignore_token(lib, proj)) {
                    add_dependency(proj, lib, DEP_BUILD);
                }
            }
            
            if (contains(trimmed, "pixman")) {
                add_dependency(proj, "pixman", DEP_BUILD);
            }
        }
    }
}

Project* parse_makefile(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    
    Project *proj = xcalloc(1, sizeof(Project));
    variables = xcalloc(16, sizeof(MakeVar));
    var_capacity = 16;
    var_count = 0;
    
    char line[MAX_LINE];
    char compound_line[MAX_LINE] = {0};
    
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim(line);
        size_t t_len = strlen(trimmed);
        
        if (t_len > 0 && trimmed[t_len - 1] == '\\') {
            trimmed[t_len - 1] = '\0';
            strlcat_safe(compound_line, trimmed, MAX_LINE);
            strlcat_safe(compound_line, " ", MAX_LINE);
            continue;
        } else {
            strlcat_safe(compound_line, trimmed, MAX_LINE);
        }
        
        if (compound_line[0] != '\t') {
            char *equal = strchr(compound_line, '=');
            if (equal) {
                *equal = '\0';
                char *name = trim(compound_line);
                char *value = trim(equal + 1);
                store_variable(name, value);
            }
        }
        compound_line[0] = '\0';
    }
    
    rewind(fp);
    compound_line[0] = '\0';
    
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim(line);
        size_t t_len = strlen(trimmed);
        
        if (t_len > 0 && trimmed[t_len - 1] == '\\') {
            trimmed[t_len - 1] = '\0';
            strlcat_safe(compound_line, trimmed, MAX_LINE);
            strlcat_safe(compound_line, " ", MAX_LINE);
            continue;
        } else {
            strlcat_safe(compound_line, trimmed, MAX_LINE);
        }
        
        parse_line(compound_line, proj);
        compound_line[0] = '\0';
    }
    
    fclose(fp);
    detect_build_system(proj);
    
    if (proj->name[0] == '\0' || strcmp(proj->name, "project") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd))) {
            char *last_slash = strrchr(cwd, '/');
            if (last_slash) {
                char *dir_name = last_slash + 1;
                char clean[MAX_NAME];
                extract_executable_name(dir_name, clean, MAX_NAME);
                if (strlen(clean) > 0 && strcmp(clean, "project") != 0) {
                    strlcpy_safe(proj->name, clean, MAX_NAME);
                    if (proj->main_target[0] == '\0' || strcmp(proj->main_target, "project") == 0) {
                        strlcpy_safe(proj->main_target, clean, MAX_NAME);
                    }
                }
            }
        }
        if (proj->name[0] == '\0' || strcmp(proj->name, "project") == 0) {
            strlcpy_safe(proj->name, "myproject", MAX_NAME);
            if (proj->main_target[0] == '\0') {
                strlcpy_safe(proj->main_target, "myproject", MAX_NAME);
            }
        }
    }
    
    free(variables);
    variables = NULL;
    var_count = 0;
    var_capacity = 0;
    
    return proj;
}

Project* parse_config_mk(const char *path) {
    if (!file_exists(path)) return NULL;
    return parse_makefile(path);
}

void free_project(Project *proj) {
    if (!proj) return;
    
    if (proj->rules) {
        for (int i = 0; i < proj->rule_count; i++) {
            if (proj->rules[i].dependencies) free(proj->rules[i].dependencies);
            if (proj->rules[i].commands) free(proj->rules[i].commands);
        }
        free(proj->rules);
    }
    
    if (proj->outputs) {
        for (int i = 0; i < proj->output_count; i++) {
            free(proj->outputs[i]);
        }
        free(proj->outputs);
    }
    
    free(proj);
}
