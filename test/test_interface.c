#include "test_interface.h"
#include "ccn/action_types.h"
#include "ccn/dynamic_core.h"
#include "ccngen/action_handling.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "src/phase_driver.c"
#include "to_string.h"
#include <ccn/phase_driver.h>
#include <stddef.h>

// What to do when a breakpoint is reached.
void BreakpointHandler(node_st *root)
{
    // Nothing
    (void)root;
    return;
}

node_st *run_scan_parse_buf(const char *filepath, char *buffer, uint32_t buffer_length)
{
    GLBinitializeGlobals();
    global.input_file = filepath;
    global.input_buf = buffer;
    global.input_buf_len = buffer_length;
    resetPhaseDriver();
    phase_driver.verbosity = PD_V_HIGH;
    node_st *node =
        CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_SPDOSCANPARSE), CCN_ROOT_TYPE, NULL, false);
    node = TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    return node;
}

node_st *run_scan_parse(const char *filepath)
{
    return run_scan_parse_buf(filepath, NULL, 0);
}

node_st *run_context_analysis_buf(const char *filepath, char *buffer, uint32_t buffer_length)
{
    node_st *node = run_scan_parse_buf(filepath, buffer, buffer_length);
    node =
        CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_CONTEXTANALYSIS), CCN_ROOT_TYPE, node, true);
    node = TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    CTIabortOnError();
    return node;
}

node_st *run_context_analysis(const char *filepath)
{
    return run_context_analysis_buf(filepath, NULL, 0);
}

node_st *run_code_generation_buf(const char *input_filepath, char *buffer, uint32_t buffer_length,
                                 const char *output_filepath, char *out_buffer,
                                 uint32_t out_buffer_length)
{
    node_st *node = run_context_analysis_buf(input_filepath, buffer, buffer_length);
    global.output_file = output_filepath;
    global.output_buf = out_buffer;
    global.output_buf_len = out_buffer_length;
    // node = CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_...), CCN_ROOT_TYPE, node, true);
    node = TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    return node;
}

node_st *run_code_generation_file(const char *input_filepath, const char *output_filepath)
{
    return run_code_generation_buf(input_filepath, NULL, 0, output_filepath, NULL, 0);
}

node_st *run_code_generation(const char *input_filepath, char *out_buffer,
                             uint32_t out_buffer_length)
{
    return run_code_generation_buf(input_filepath, NULL, 0, "internal buffer", out_buffer,
                                   out_buffer_length);
}

node_st *run_code_generation_node(node_st *node, char *out_buffer, uint32_t out_buffer_length)
{
    GLBinitializeGlobals();
    global.output_file = "internal buffer";
    global.output_buf = out_buffer;
    global.output_buf_len = out_buffer_length;
    resetPhaseDriver();
    // node = CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_...), NODE_TYPE(node), node, false);
    node = TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    return node;
}

void cleanup_nodes(node_st *root)
{
    root = CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_FREESYMBOLS), CCN_ROOT_TYPE, root, true);
    TRAVstart(root, TRAV_free);
}
