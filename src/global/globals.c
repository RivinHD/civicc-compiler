#include "globals.h"

struct globals global;

/*
 * Initialize global variables from globals.mac
 */

void GLBinitializeGlobals()
{
    global.col = 1;
    global.line = 1;
    global.input_file = NULL;
    global.output_file = NULL;
}
