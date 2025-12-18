#pragma once

#include <stdbool.h>
#include <stddef.h>

struct globals
{
    int line;
    int col;
    int verbose;
    const char *input_file;
    const char *output_file;
};

extern struct globals global;
extern void GLBinitializeGlobals(void);
