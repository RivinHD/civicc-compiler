#include "globals.h"
#include <stdio.h>

struct globals global;

/*
 * Initialize global variables from globals.mac
 */

void GLBinitializeGlobals()
{
    global.preprocessor_enabled = true;
    global.optimization_enabled = true;
    global.col = 1;
    global.line = 1;
    global.input_buf_len = 0;
    global.input_buf = NULL;
    global.input_file = NULL;
    global.filename = NULL;
    global.output_buf_len = 0;
    global.output_buf = NULL;
    global.output_file = NULL;
    global.default_out_stream = stdout;
}

bool isOptimizationEnabled()
{
    return global.optimization_enabled;
}
