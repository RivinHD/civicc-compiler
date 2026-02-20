#include "test_interface.h"
#include "ccn/action_types.h"
#include "ccn/dynamic_core.h"
#include "ccngen/action_handling.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
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
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    phase_driver.verbosity = PD_V_HIGH;
#endif
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
    if (CTIgetErrors() > 0)
    {
        cleanup_nodes(node);
    }
    CTIabortOnError();
    return node;
}

node_st *run_context_analysis(const char *filepath)
{
    return run_context_analysis_buf(filepath, NULL, 0);
}

node_st *run_optimization_buf(const char *filepath, char *buffer, uint32_t buffer_length,
                              enum ccn_action_id opt_id)
{
    node_st *node = run_code_gen_preparation_buf(filepath, buffer, buffer_length);
    node = CCNdispatchAction(CCNgetActionFromID(opt_id), CCN_ROOT_TYPE, node, true);
    node = TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    CTIabortOnError();
    return node;
}

node_st *run_optimization(const char *filepath, enum ccn_action_id opt_id)
{
    return run_optimization_buf(filepath, NULL, 0, opt_id);
}

node_st *run_code_gen_preparation_buf(const char *filepath, char *buffer, uint32_t buffer_length)
{
    node_st *node = run_context_analysis_buf(filepath, buffer, buffer_length);
    node = CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_CODEGENPREPARATION), CCN_ROOT_TYPE, node,
                             true);
    node = TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    if (CTIgetErrors() > 0)
    {
        cleanup_nodes(node);
    }
    CTIabortOnError();
    return node;
}

node_st *run_code_gen_preparation(const char *filepath)
{
    return run_code_gen_preparation_buf(filepath, NULL, 0);
}

node_st *run_code_generation_buf(const char *input_filepath, char *buffer, uint32_t buffer_length,
                                 const char *output_filepath, char *out_buffer,
                                 uint32_t out_buffer_length, bool optimize)
{
    node_st *node = optimize ? run_optimization_buf(input_filepath, buffer, buffer_length,
                                                    CCNAC_ID_OPTIMIZATION)
                             : run_code_gen_preparation_buf(input_filepath, buffer, buffer_length);

    global.output_file = output_filepath;
    global.output_buf = out_buffer;
    global.output_buf_len = out_buffer_length;
    node = CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_CODEGEN), CCN_ROOT_TYPE, node, true);
    node = TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    return node;
}

node_st *run_code_generation_file(const char *input_filepath, const char *output_filepath,
                                  bool optimize)
{
    return run_code_generation_buf(input_filepath, NULL, 0, output_filepath, NULL, 0, optimize);
}

node_st *run_code_generation(const char *input_filepath, char *out_buffer,
                             uint32_t out_buffer_length, bool optimize)
{
    return run_code_generation_buf(input_filepath, NULL, 0, NULL, out_buffer, out_buffer_length,
                                   optimize);
}

node_st *run_code_generation_node(node_st *node, char *out_buffer, uint32_t out_buffer_length)
{
    GLBinitializeGlobals();
    global.output_file = NULL;
    global.output_buf = out_buffer;
    global.output_buf_len = out_buffer_length;
    resetPhaseDriver();
    node = CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_CODEGEN), NODE_TYPE(node), node, false);
    node = TRAVstart(node, TRAV_check); // Check for inconstientcies in the AST
    return node;
}

void cleanup_nodes(node_st *root)
{
    root = CCNdispatchAction(CCNgetActionFromID(CCNAC_ID_FREESYMBOLS), CCN_ROOT_TYPE, root, true);
    TRAVstart(root, TRAV_free);
}
