#ifndef PARSER_H
#define PARSER_H

#include "make2flake.h"

Project* parse_makefile(const char *path);
Project* parse_config_mk(const char *path);
void free_project(Project *proj);

#endif
