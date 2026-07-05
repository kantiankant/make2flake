#include "dependency.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char name[MAX_NAME];
    char attr[MAX_NAME];
    DepType type;
    bool resolved;
} DepCache;

static DepCache cache[256];
static int cache_count = 0;
static bool nix_locate_available = false;
static bool nix_locate_checked = false;

static bool check_nix_locate(void) {
    if (nix_locate_checked) return nix_locate_available;
    nix_locate_checked = true;
    
    FILE *fp = popen("command -v nix-locate 2>/dev/null", "r");
    if (!fp) return false;
    
    char result[256];
    if (fgets(result, sizeof(result), fp) != NULL && strlen(result) > 0) {
        nix_locate_available = true;
    }
    pclose(fp);
    
    if (!nix_locate_available) {
        fprintf(stderr, "Warning: nix-locate not found in PATH\n");
        fprintf(stderr, "  Install it with: nix profile install nixpkgs#nix-index\n");
        fprintf(stderr, "  Or: nix-env -iA nixpkgs.nix-index\n");
        fprintf(stderr, "  Without nix-locate, dependency resolution will be limited\n");
    }
    
    return nix_locate_available;
}

static bool query_nix_locate(const char *mod_name, char *out_attr, size_t max_len, DepType *type) {
    if (!check_nix_locate()) {
        return false;
    }
    
    char safe_mod[MAX_NAME];
    char command[1024];
    char result[256];
    
    /* sanitize input to only allow alnum, dash, underscore, dot, etc etc blah blah blah i'm tired of documenting ts :sob: */
    int j = 0;
    for (int i = 0; mod_name[i] && j < MAX_NAME-1; i++) {
        if (isalnum(mod_name[i]) || mod_name[i] == '-' || 
            mod_name[i] == '_' || mod_name[i] == '.') {
            safe_mod[j++] = mod_name[i];
        }
    }
    safe_mod[j] = '\0';
    
    if (strlen(safe_mod) == 0) return false;
    
    /* try multiple search patterns */
    const char *patterns[] = {
        "/bin/%s",                    /* bin tools */
        "/lib/pkgconfig/%s.pc",       /* pkg-config files for libs */
        "/share/pkgconfig/%s.pc",     /* arch-indep pkg-config */
        "/include/%s",               /* header files */
        "/lib/lib%s.so",             /* .so (shared libs) */
        "/lib/lib%s.a",              /* .a (static libraries) */
        NULL
    };
    
    for (int p = 0; patterns[p] != NULL; p++) {
        char path[512];
        snprintf(path, sizeof(path), patterns[p], safe_mod);
        
        snprintf(command, sizeof(command), 
                 "nix-locate --top-level --minimal --at-root '%s' 2>/dev/null | head -n 1", 
                 path);
        
        FILE *fp = popen(command, "r");
        if (!fp) continue;
        
        if (fgets(result, sizeof(result), fp)) {
            trim(result);
            if (strlen(result) > 0) {
                /* determine type based on where it was found */
                if (strstr(patterns[p], "/bin/") != NULL) {
                    *type = DEP_NATIVE;
                } else {
                    *type = DEP_BUILD;
                }
                
                /* extract just the pkg name from the result */
                char *dot = strchr(result, '.');
                if (dot) {
                    strlcpy_safe(out_attr, dot + 1, max_len);
                } else {
                    strlcpy_safe(out_attr, result, max_len);
                }
                
                pclose(fp);
                return true;
            }
        }
        pclose(fp);
    }
    
    return false;
}

static bool cache_lookup(const char *name, char *out_attr, DepType *type) {
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(cache[i].name, name) == 0) {
            if (cache[i].resolved) {
                strlcpy_safe(out_attr, cache[i].attr, MAX_NAME);
                *type = cache[i].type;
                return true;
            }
            return false;
        }
    }
    return false;
}

static void cache_store(const char *name, const char *attr, DepType type, bool resolved) {
    if (cache_count >= 256) return;
    
    strlcpy_safe(cache[cache_count].name, name, MAX_NAME);
    if (resolved) {
        strlcpy_safe(cache[cache_count].attr, attr, MAX_NAME);
        cache[cache_count].type = type;
    }
    cache[cache_count].resolved = resolved;
    cache_count++;
}

bool resolve_dependency(const char *name, char *out_attr, size_t max_len, DepType *type) {
    if (!name || !out_attr || !type) return false;
    
    /* cache check */
    if (cache_lookup(name, out_attr, type)) {
        return true;
    }
    
    /* nix-locate test */
    char attr[MAX_NAME];
    DepType found_type = DEP_BUILD;
    
    if (query_nix_locate(name, attr, MAX_NAME, &found_type)) {
        strlcpy_safe(out_attr, attr, max_len);
        *type = found_type;
        cache_store(name, attr, found_type, true);
        return true;
    }
    
    /* if nix-locate failed, try common patterns as a last resort */
    
    /* check if it's a common tool that might be in PATH (probably not) */
    if (strcmp(name, "make") == 0 || strcmp(name, "gmake") == 0) {
        strlcpy_safe(out_attr, "gnumake", max_len);
        *type = DEP_NATIVE;
        cache_store(name, "gnumake", DEP_NATIVE, true);
        return true;
    }
    
    /* check if it looks like a lib name (ends with common suffixes) */
    if (strlen(name) >= 3) {
        /* try to guess based on common patterns */
        char guess[MAX_NAME];
        strlcpy_safe(guess, name, max_len);
        
        /* rm common prefixes */
        if (strncmp(guess, "lib", 3) == 0) {
            memmove(guess, guess + 3, strlen(guess) - 2);
        }
        
        /* if it contains a version, it gets stripped */
        char *dash = strchr(guess, '-');
        if (dash) {
            *dash = '\0';
        }
        
        /* replace . with _ for nix attribute names */
        for (char *p = guess; *p; p++) {
            if (*p == '.') *p = '_';
        }
        
        if (strlen(guess) > 0) {
            strlcpy_safe(out_attr, guess, max_len);
            *type = DEP_BUILD;
            cache_store(name, guess, DEP_BUILD, true);
            return true;
        }
    }
    
    /* not found = skip dependency */
    cache_store(name, "", DEP_BUILD, false);
    return false;
}

void resolve_all_dependencies(Project *proj) {
    if (!proj) return;
    
    for (int i = 0; i < proj->dep_count; i++) {
        if (!proj->deps[i].resolved) {
            char attr[MAX_NAME];
            DepType type;
            if (resolve_dependency(proj->deps[i].name, attr, MAX_NAME, &type)) {
                strlcpy_safe(proj->deps[i].nix_attr, attr, MAX_NAME);
                proj->deps[i].type = type;
                proj->deps[i].resolved = true;
            }
        }
    }
}

bool add_dependency(Project *proj, const char *name, DepType type) {
    if (!proj || !name) return false;
    if (proj->dep_count >= MAX_DEPS) return false;
    
    /* check for duplicates (it's case insensitive) */
    for (int i = 0; i < proj->dep_count; i++) {
        if (strcasecmp(proj->deps[i].name, name) == 0) {
            return true;
        }
    }
    
    strlcpy_safe(proj->deps[proj->dep_count].name, name, MAX_NAME);
    proj->deps[proj->dep_count].type = type;
    proj->deps[proj->dep_count].resolved = false;
    proj->dep_count++;
    
    return true;
}
