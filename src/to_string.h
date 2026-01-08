#pragma once

#include "ccngen/ast.h"
#include "ccngen/enum.h"

char *datatype_to_string(enum DataType type);
char *monoptype_to_string(enum MonOpType type);
char *binoptype_to_string(enum BinOpType type);
char *_node_to_string(node_st *node, unsigned int depth, const char *connection,
                      char *depth_string);
char *node_to_string(node_st *node);
char *_funheader_params_to_oneliner_string(node_st *node);
char *symbols_to_string(node_st *node);
