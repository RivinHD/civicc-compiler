#include "ccn/action_types.h"
#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccn/phase_driver.h"
#include "ccngen/action_handling.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "palm/dbug.h"
#include "palm/str.h"
#include "src/phase_driver.c"
#include <stdio.h>

// What to do when a breakpoint is reached.
void BreakpointHandler(node_st *root)
{
    // Nothing
    (void)root;
    return;
}

node_st *run_scan_parse(char *filepath)
{
    GLBinitializeGlobals();
    global.input_file = filepath;
    resetPhaseDriver();
    node_st *node =
        CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_SPDOSCANPARSE), CCN_ROOT_TYPE, NULL, false);
    return node;
}

void cleanup_scan_parse(node_st *root)
{
    TRAVstart(root, TRAV_free);
}
