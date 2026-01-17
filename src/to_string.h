#pragma once

#include "ccngen/ast.h"
#include "ccngen/enum.h"

char *datatype_to_string(enum DataType type);
char *monoptype_to_string(enum MonOpType type);
char *binoptype_to_string(enum BinOpType type);
bool has_child_next(node_st *node, node_st **child_next);
char *_node_to_string_array(node_st *node, unsigned int depth, char *depth_string,
                            bool is_last_child);
char *get_node_name(node_st *node);
char *_node_to_string(node_st *node, unsigned int depth, const char *connection,
                      char *depth_string);
char *node_to_string(node_st *node);
char *_funheader_params_to_oneliner_string(node_st *node);
char *_symbols_to_string(node_st *node, htable_stptr htable, uint32_t counter);
char *symbols_to_string(node_st *node);
