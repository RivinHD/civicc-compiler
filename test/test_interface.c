#include "ccn/action_types.h"
#include "ccn/dynamic_core.h"
#include "ccngen/action_handling.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "global/globals.h"
#include "src/phase_driver.c"

// What to do when a breakpoint is reached.
void BreakpointHandler(node_st *root)
{
    // Nothing
    (void)root;
    return;
}

node_st *run_scan_parse_buf(const char *filepath, char *buffer, int buffer_length)
{
    GLBinitializeGlobals();
    global.input_file = filepath;
    global.input_buf = buffer;
    global.input_buf_len = buffer_length;
    resetPhaseDriver();
    node_st *node =
        CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_SPDOSCANPARSE), CCN_ROOT_TYPE, NULL, false);
    TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    return node;
}

node_st *run_scan_parse(const char *filepath)
{
    return run_scan_parse_buf(filepath, NULL, 0);
}

void cleanup_scan_parse(node_st *root)
{
    TRAVstart(root, TRAV_free);
}
