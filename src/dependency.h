#ifndef DEPENDENCY_H
#define DEPENDENCY_H

#include "make2flake.h"
#include <stdbool.h>
#include <stddef.h>

bool resolve_dependency(const char *name, char *out_attr, size_t max_len, DepType *type);
void resolve_all_dependencies(Project *proj);
bool add_dependency(Project *proj, const char *name, DepType type);

#endif
