#include "make2flake.h"
#include "parser.h"
#include "generator.h"
#include "dependency.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define VERSION "0.1.0"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -i, --input <file>     Input Makefile (default: Makefile)\n");
    fprintf(stderr, "  -c, --config <file>    Additional config.mk file\n");
    fprintf(stderr, "  -o, --output <file>    Output flake.nix (default: stdout)\n");
    fprintf(stderr, "  -n, --name <name>      Project name (auto-detected if not set)\n");
    fprintf(stderr, "  -h, --help             Show this help\n");
    fprintf(stderr, "  --version              Show version\n");
    fprintf(stderr, "  --verbose              Verbose output\n");
}

static void print_version(void) {
    fprintf(stderr, "make2flake version %s\n", VERSION);
    fprintf(stderr, "Convert Makefiles to Nix flakes\n");
}

int main(int argc, char *argv[]) {
    const char *makefile_path = "Makefile";
    const char *config_path = NULL;
    const char *output_path = NULL;
    const char *project_name = NULL;
    const char *project_version = NULL;
    bool verbose = false;
    
    /* parse command line */
    struct option long_opts[] = {
        {"input", required_argument, 0, 'i'},
        {"config", required_argument, 0, 'c'},
        {"output", required_argument, 0, 'o'},
        {"name", required_argument, 0, 'n'},
        {"project-version", required_argument, 0, 'V'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 1000},
        {"verbose", no_argument, 0, 1001},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "i:c:o:n:V:h", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'i': makefile_path = optarg; break;
            case 'c': config_path = optarg; break;
            case 'o': output_path = optarg; break;
            case 'n': project_name = optarg; break;
            case 'V': project_version = optarg; break;
            case 'h': print_usage(argv[0]); return 0;
            case 1000: print_version(); return 0;
            case 1001: verbose = true; break;
            default: print_usage(argv[0]); return 1;
        }
    }
    
    if (verbose) {
        fprintf(stderr, "Parsing Makefile: %s\n", makefile_path);
    }
    
    /* parse main makefile */
    Project *proj = parse_makefile(makefile_path);
    if (!proj) {
        fprintf(stderr, "Error: Failed to parse %s\n", makefile_path);
        return 1;
    }
    
    /* parse config.mk if provided */
    if (config_path) {
        if (verbose) fprintf(stderr, "Parsing config: %s\n", config_path);
        Project *config = parse_config_mk(config_path);
        if (config) {
            if (config->name[0]) strlcpy_safe(proj->name, config->name, MAX_NAME);
            if (config->version[0]) strlcpy_safe(proj->version, config->version, MAX_NAME);
            if (config->main_target[0]) strlcpy_safe(proj->main_target, config->main_target, MAX_NAME);
            if (config->install_target[0]) strlcpy_safe(proj->install_target, config->install_target, MAX_NAME);
            
            for (int i = 0; i < config->dep_count && proj->dep_count < MAX_DEPS; i++) {
                add_dependency(proj, config->deps[i].name, config->deps[i].type);
            }
            
            free_project(config);
        }
    }
    
    /* override from command line */
    if (project_name) strlcpy_safe(proj->name, project_name, MAX_NAME);
    if (project_version) strlcpy_safe(proj->version, project_version, MAX_NAME);
    
    if (verbose) {
        fprintf(stderr, "Project: %s\n", proj->name);
        fprintf(stderr, "Version: %s\n", proj->version);
        fprintf(stderr, "Main target: %s\n", proj->main_target);
        fprintf(stderr, "Dependencies found: %d\n", proj->dep_count);
    }
    
    /* generate flake */
    if (verbose) fprintf(stderr, "Generating flake.nix...\n");
    
    if (!generate_flake(proj, output_path)) {
        fprintf(stderr, "Error: Failed to generate flake.nix\n");
        free_project(proj);
        return 1;
    }
    
    if (verbose) {
        if (output_path) {
            fprintf(stderr, "Generated %s successfully\n", output_path);
        } else {
            fprintf(stderr, "Generated flake.nix to stdout\n");
        }
    }
    
    free_project(proj);
    return 0;
}
