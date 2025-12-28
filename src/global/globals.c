#include "globals.h"

struct globals global;

/*
 * Initialize global variables from globals.mac
 */

void GLBinitializeGlobals()
{
    global.col = 1;
    global.line = 1;
    global.input_buf_len = 0;
    global.input_buf = NULL;
    global.input_file = NULL;
    global.output_buf_len = 0;
    global.output_buf = NULL;
    global.output_file = NULL;
}
