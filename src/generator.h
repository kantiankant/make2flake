#ifndef GENERATOR_H
#define GENERATOR_H

#include "make2flake.h"
#include <stdbool.h>

bool generate_flake(const Project *proj, const char *output_path);

#endif
