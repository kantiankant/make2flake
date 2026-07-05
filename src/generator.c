#include "generator.h"
#include "dependency.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_header(FILE *out, const Project *proj) {
    fprintf(out, "{\n");
    fprintf(out, "  description = \"Generated flake for %s\";\n\n", 
            proj->name[0] ? proj->name : "project");
    
    fprintf(out, "  inputs = {\n");
    fprintf(out, "    nixpkgs.url = \"github:NixOS/nixpkgs/nixos-unstable\";\n");
    fprintf(out, "    flake-utils.url = \"github:numtide/flake-utils\";\n");
    fprintf(out, "  };\n\n");
}

static void write_outputs_start(FILE *out, const Project *proj) {
    (void)proj;
    
    fprintf(out, "  outputs = { self, nixpkgs, flake-utils }:\n");
    fprintf(out, "    flake-utils.lib.eachDefaultSystem (system:\n");
    fprintf(out, "      let\n");
    fprintf(out, "        pkgs = nixpkgs.legacyPackages.${system};\n");
    fprintf(out, "      in {\n");
    fprintf(out, "        packages = {\n");
    fprintf(out, "          default = pkgs.stdenv.mkDerivation {\n");
}

static void write_package_body(FILE *out, const Project *proj) {
    fprintf(out, "            pname = \"%s\";\n", proj->name[0] ? proj->name : "project");
    fprintf(out, "            version = \"%s\";\n", proj->version[0] ? proj->version : "0.1.0");
    fprintf(out, "            src = self;\n\n");
    
    fprintf(out, "            enableParallelBuilding = true;\n\n");
    
    /* nativeBuildInputs */
    fprintf(out, "            nativeBuildInputs = with pkgs; [\n");
    
    bool has_native = false;
    /* Use a set to track added deps to avoid duplicates */
    char added[MAX_DEPS][MAX_NAME];
    int added_count = 0;
    
    for (int i = 0; i < proj->dep_count; i++) {
        if (proj->deps[i].type == DEP_NATIVE && proj->deps[i].resolved) {
            /* check if already added */
            bool already = false;
            for (int j = 0; j < added_count; j++) {
                if (strcmp(added[j], proj->deps[i].nix_attr) == 0) {
                    already = true;
                    break;
                }
            }
            if (!already && strlen(proj->deps[i].nix_attr) > 0) {
                fprintf(out, "              %s\n", proj->deps[i].nix_attr);
                strlcpy_safe(added[added_count++], proj->deps[i].nix_attr, MAX_NAME);
                has_native = true;
            }
        }
    }
    
    if (proj->has_autotools) {
        fprintf(out, "              autoconf\n");
        fprintf(out, "              automake\n");
        fprintf(out, "              autoconf-archive\n");
        has_native = true;
    }
    if (proj->has_cmake) {
        fprintf(out, "              cmake\n");
        has_native = true;
    }
    if (proj->has_meson) {
        fprintf(out, "              meson\n");
        fprintf(out, "              ninja\n");
        has_native = true;
    }
    if (proj->has_pkg_config) {
        fprintf(out, "              pkg-config\n");
        has_native = true;
    }
    if (proj->has_python) {
        fprintf(out, "              python3\n");
        fprintf(out, "              python3Packages.setuptools\n");
        has_native = true;
    }
    
    if (!has_native) {
        fprintf(out, "              # No native dependencies detected\n");
    }
    
    fprintf(out, "            ];\n\n");
    
    /* buildInputs */
    fprintf(out, "            buildInputs = with pkgs; [\n");
    bool has_build = false;
    added_count = 0;
    
    for (int i = 0; i < proj->dep_count; i++) {
        if (proj->deps[i].type == DEP_BUILD && proj->deps[i].resolved) {
            bool already = false;
            for (int j = 0; j < added_count; j++) {
                if (strcmp(added[j], proj->deps[i].nix_attr) == 0) {
                    already = true;
                    break;
                }
            }
            if (!already && strlen(proj->deps[i].nix_attr) > 0) {
                fprintf(out, "              %s\n", proj->deps[i].nix_attr);
                strlcpy_safe(added[added_count++], proj->deps[i].nix_attr, MAX_NAME);
                has_build = true;
            }
        }
    }
    
    if (!has_build) {
        fprintf(out, "              # No build dependencies detected\n");
    }
    fprintf(out, "            ];\n\n");
    
    /* buildPhase */
    fprintf(out, "            buildPhase = ''\n");
    if (proj->has_autotools) {
        fprintf(out, "              ./configure --prefix=$out\n");
        fprintf(out, "              make -j$NIX_BUILD_CORES\n");
    } else if (proj->has_cmake) {
        fprintf(out, "              cmake -DCMAKE_INSTALL_PREFIX=$out .\n");
        fprintf(out, "              make -j$NIX_BUILD_CORES\n");
    } else if (proj->has_meson) {
        fprintf(out, "              meson setup build\n");
        fprintf(out, "              ninja -C build\n");
    } else {
        fprintf(out, "              make -j$NIX_BUILD_CORES\n");
    }
    fprintf(out, "            '';\n\n");
    
    /* installPhase */
    fprintf(out, "            installPhase = ''\n");
    if (proj->install_target[0]) {
        fprintf(out, "              make install DESTDIR=$out PREFIX=$out\n");
    } else if (proj->main_target[0]) {
        fprintf(out, "              mkdir -p $out/bin\n");
        fprintf(out, "              cp %s $out/bin/\n", proj->main_target);
    } else {
        fprintf(out, "              mkdir -p $out/bin\n");
        fprintf(out, "              echo \"Warning: No install target detected\"\n");
    }
    fprintf(out, "            '';\n\n");
    
    /* makeFlags */
    fprintf(out, "            makeFlags = [\n");
    fprintf(out, "              \"PREFIX=${placeholder \"out\"}\"\n");
    fprintf(out, "              \"DESTDIR=\"\n");
    if (proj->cflags[0]) {
        fprintf(out, "              \"CFLAGS=%s\"\n", proj->cflags);
    }
    if (proj->ldflags[0]) {
        fprintf(out, "              \"LDFLAGS=%s\"\n", proj->ldflags);
    }
    fprintf(out, "            ];\n\n");
    
    /* meta section */
    fprintf(out, "            meta = with pkgs.lib; {\n");
    fprintf(out, "              description = \"%s\";\n", 
            proj->name[0] ? proj->name : "Generated from Makefile");
    fprintf(out, "              platforms = platforms.linux;\n");
    if (proj->main_target[0]) {
        fprintf(out, "              mainProgram = \"%s\";\n", proj->main_target);
    }
    fprintf(out, "            };\n");
    fprintf(out, "          };\n");
    fprintf(out, "        };\n\n");
}

static void write_devshell(FILE *out, const Project *proj) {
    fprintf(out, "        devShells = {\n");
    fprintf(out, "          default = pkgs.mkShell {\n");
    fprintf(out, "            inputsFrom = [ self.packages.${system}.default ];\n");
    fprintf(out, "            packages = with pkgs; [\n");
    fprintf(out, "              pkg-config\n");
    fprintf(out, "              gdb\n");
    fprintf(out, "            ];\n");
    fprintf(out, "            shellHook = ''\n");
    fprintf(out, "              echo \"Development environment for %s\"\n", proj->name);
    fprintf(out, "            '';\n");
    fprintf(out, "          };\n");
    fprintf(out, "        };\n\n");
    
    fprintf(out, "        apps.default = flake-utils.lib.mkApp {\n");
    fprintf(out, "          drv = self.packages.${system}.default;\n");
    fprintf(out, "        };\n");
}

bool generate_flake(const Project *proj, const char *output_path) {
    FILE *out = stdout;
    if (output_path) {
        out = fopen(output_path, "w");
        if (!out) {
            fprintf(stderr, "Error: Cannot open %s for writing\n", output_path);
            return false;
        }
    }
    
    resolve_all_dependencies((Project*)proj);
    
    write_header(out, proj);
    write_outputs_start(out, proj);
    write_package_body(out, proj);
    write_devshell(out, proj);
    
    fprintf(out, "      }\n");
    fprintf(out, "    );\n");
    fprintf(out, "}\n");
    
    if (output_path) fclose(out);
    return true;
}
