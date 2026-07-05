#ifndef MAKE2FLAKE_H
#define MAKE2FLAKE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LINE 8192
#define MAX_NAME 256
#define MAX_DEPS 128

/* Dependency types for Nix */
typedef enum {
    DEP_NATIVE,   /* nativeBuildInputs (build-time tools) */
    DEP_BUILD,    /* buildInputs (runtime libs) */
    DEP_CHECK,    /* checkInputs (test deps) */
    DEP_DEV       /* devInputs (dev tools) */
} DepType;

/* Single dependency */
typedef struct {
    char name[MAX_NAME];
    char nix_attr[MAX_NAME];  /* the actual nix attribute name */
    DepType type;
    bool resolved;
} Dependency;

/* Build rule from Makefile */
typedef struct {
    char target[MAX_NAME];
    char **dependencies;      
    int dep_count;
    char **commands;         
    int cmd_count;
    bool is_phony;
} BuildRule;

/* project representation */
typedef struct {
    char name[MAX_NAME];
    char version[MAX_NAME];
    char main_target[MAX_NAME];
    char install_target[MAX_NAME];
    
    /* deps */
    Dependency deps[MAX_DEPS];
    int dep_count;
    
    /* build rules */
    BuildRule *rules;
    int rule_count;
    
    /* build sys detection */
    bool has_autotools;
    bool has_cmake;
    bool has_meson;
    bool has_pkg_config;
    bool has_python;
    bool uses_guile;
    
    /* build flags from config.mk */
    char cflags[MAX_LINE];
    char ldflags[MAX_LINE];
    char make_flags[MAX_LINE];
    
    /* output files detected */
    char **outputs;
    int output_count;
} Project;

/* Parser functions */
Project* parse_makefile(const char *path);
Project* parse_config_mk(const char *path);
void free_project(Project *proj);

/* dep resolution */
bool resolve_dependency(const char *name, char *out_attr, size_t max_len, DepType *type);
void resolve_all_dependencies(Project *proj);
bool add_dependency(Project *proj, const char *name, DepType type);

/* generator */
bool generate_flake(const Project *proj, const char *output_path);

#endif
