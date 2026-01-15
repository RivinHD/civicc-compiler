#pragma once

#include "ccngen/enum.h"
#include "context_analysis/definitions.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "palm/str.h"
#include "release_assert.h"
#include <ccngen/ast.h>
#include <stdbool.h>
#include <stdint.h>

#include <stdarg.h>
#include <stdio.h>

#ifdef DEBUG_SCANPARSE
#define scanparse_fprintf(stream, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        fprintf((stream), __VA_ARGS__);                                                            \
    } while (0)
#else
#define scanparse_fprintf(...) ((void)0)
#endif

static inline uint64_t nodessettype_to_nodetypes(enum nodesettype type)
{
    uint64_t one = 1;
    switch (type)
    {
    case NS_LINK:
        return nodessettype_to_nodetypes(NS_EXPR) | nodessettype_to_nodetypes(NS_STATEMENT);
    case NS_EXPROPTARRAYINIT:
        return nodessettype_to_nodetypes(NS_EXPR) | (one << NT_ARRAYINIT);
    case NS_VAROPTARRAYEXPR:
        return (one << NT_VAR) | (one << NT_ARRAYEXPR);
    case NS_VAROPTARRAYVAR:
        return (one << NT_VAR) | (one << NT_ARRAYVAR);
    case NS_DECLARATION:
        return (one << NT_FUNDEC) | (one << NT_FUNDEF) | (one << NT_GLOBALDEC) |
               (one << NT_GLOBALDEF);
    case NS_STATEMENT:
        return (one << NT_ASSIGN) | (one << NT_PROCCALL) | (one << NT_IFSTATEMENT) |
               (one << NT_WHILELOOP) | (one << NT_DOWHILELOOP) | (one << NT_FORLOOP) |
               (one << NT_FORLOOP) | (one << NT_RETSTATEMENT) | (one << NT_ARRAYASSIGN);
    case NS_EXPR:
        return (one << NT_BINOP) | (one << NT_MONOP) | (one << NT_CAST) | (one << NT_PROCCALL) |
               (one << NT_VAR) | (one << NT_ARRAYEXPR) | (one << NT_INT) | (one << NT_FLOAT) |
               (one << NT_BOOL);
    case NS_NULL:
        return 0;
    case _NS_SIZE:
        // Not Convertible
        release_assert(false);
    }

    // Undefined type
    release_assert(false);
}

#define NODE_TO_CTINFO(node)                                                                       \
    {                                                                                              \
        (int)NODE_BLINE((node)),                                                                   \
        (int)NODE_BCOL((node)),                                                                    \
        (int)NODE_ELINE((node)),                                                                   \
        (int)NODE_ECOL((node)),                                                                    \
        NULL,                                                                                      \
        NULL,                                                                                      \
    }

#define assertSetType(node, setType)                                                               \
    do                                                                                             \
    {                                                                                              \
        node_st *_ast_node = (node);                                                               \
        if (_ast_node == NULL)                                                                     \
            break;                                                                                 \
        scanparse_fprintf(stdout, "Set: %d %d\n", NODE_TYPE(_ast_node), (int)(setType));           \
        uint64_t _ast_combined = nodessettype_to_nodetypes((setType));                             \
        release_assert(((1ull << NODE_TYPE(_ast_node)) & _ast_combined) != 0);                     \
    } while (0)

#define assertType(node, type)                                                                     \
    do                                                                                             \
    {                                                                                              \
        node_st *_at_node = (node);                                                                \
        if (_at_node == NULL)                                                                      \
            break;                                                                                 \
        scanparse_fprintf(stdout, "Type: %d %d\n", NODE_TYPE(_at_node), (int)(type));              \
        release_assert(NODE_TYPE(_at_node) == (type));                                             \
    } while (0)

static inline void error_already_defined(node_st *node, node_st *found, const char *name)
{
    struct ctinfo info = NODE_TO_CTINFO(node);
    info.filename = STRcpy(global.input_file);
    CTIobj(CTI_ERROR, true, info, "'%s' already defined at %d:%d - %d:%d.", name, NODE_BLINE(found),
           NODE_BCOL(found), NODE_ELINE(found), NODE_ECOL(found));
    free(info.filename);
}

static inline void error_invalid_identifier_name(node_st *node, node_st *found, const char *name)
{
    struct ctinfo info = NODE_TO_CTINFO(node);
    info.filename = STRcpy(global.input_file);
    CTIobj(CTI_ERROR, true, info,
           "'%s' is not allowed to start with '_'. Defined at %d:%d - %d:%d.", name,
           NODE_BLINE(found), NODE_BCOL(found), NODE_ELINE(found), NODE_ECOL(found));
    free(info.filename);
}

/// Recursive lookup into the symbol table and in all parent symbol table for the given name.
static inline node_st *deep_lookup(htable_stptr htable, const char *name)
{
    node_st *entry = HTlookup(htable, (void *)name);
    while (entry == NULL)
    {
        htable_stptr parent = HTlookup(htable, htable_parent_name);
        if (parent == NULL)
        {
            break;
        }

        htable = parent;
        entry = HTlookup(htable, (void *)name);
    }

    return entry;
}

/// Extracts the type from an entry in the symbol table.
static inline enum DataType symbol_to_type(node_st *entry)
{
    switch (NODE_TYPE(entry))
    {
    case NT_VARDEC:
        return VARDEC_TYPE(entry);
    case NT_GLOBALDEC:
        return GLOBALDEC_TYPE(entry);
    case NT_FUNHEADER:
        return FUNHEADER_TYPE(entry);
    case NT_PARAMS:
        return PARAMS_TYPE(entry);
    default:
        release_assert(false);
        return DT_NULL;
    }
}
