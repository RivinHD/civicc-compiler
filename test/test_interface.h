#pragma once

#include <ccngen/ast.h>

node_st *run_scan_parse_buf(const char *filepath, char *buffer, uint32_t buffer_length);
node_st *run_scan_parse(const char *filepath);
node_st *run_context_analysis_buf(const char *filepath, char *buffer, uint32_t buffer_length);
node_st *run_context_analysis(const char *filepath);
node_st *run_code_generation_buf(const char *input_filepath, char *buffer, uint32_t buffer_length,
                                 const char *output_filepath, char *out_buffer,
                                 uint32_t out_buffer_length);
node_st *run_code_generation_file(const char *input_filepath, const char *output_filepath);
node_st *run_code_generation(const char *input_filepath, char *out_buffer,
                             uint32_t out_buffer_length);
node_st *run_code_generation_node(node_st *node, char *out_buffer, uint32_t out_buffer_length);
void cleanup_nodes(node_st *root);
