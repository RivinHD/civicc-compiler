#pragma once

#include "ccngen/enum.h"
#include "definitions.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "palm/str.h"
#include "release_assert.h"
#include <ccngen/ast.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <stdarg.h>
#include <string.h>

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

/// Recursive lookup into the symbol table and in all parent symbol table for the given name.
/// Also defines the level at which the index was found.
// The (negated/negative level) - 1 indicates that is in global scope i.e. out_level = -level - 1
static inline node_st *deep_lookup_level(htable_stptr htable, const char *name, int *out_level)
{
    int level = 0;
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
        level++;
    }

    htable_stptr parent = HTlookup(htable, htable_parent_name);
    *out_level = (parent == NULL) ? -level - 1 : level;
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
    case NT_FUNDEF:
        return FUNHEADER_TYPE(FUNDEF_FUNHEADER(entry));
    case NT_FUNDEC:
        return FUNHEADER_TYPE(FUNDEC_FUNHEADER(entry));
    case NT_PARAMS:
        return PARAMS_TYPE(entry);
    case NT_DIMENSIONVARS:
        return DT_int;
    default:
        release_assert(false);
        return DT_NULL;
    }
}

/// Extracts the var from an entry in the symbol table.
/// Function entries are not supported.
static node_st *get_var_from_symbol(node_st *entry)
{
    release_assert(entry != NULL);
    switch (NODE_TYPE(entry))
    {
    case NT_VARDEC:
        return VARDEC_VAR(entry);
    case NT_PARAMS:
        return PARAMS_VAR(entry);
    case NT_GLOBALDEC:
        return GLOBALDEC_VAR(entry);
    case NT_DIMENSIONVARS:
        return DIMENSIONVARS_DIM(entry);
    default:
        release_assert(false);
        break;
    }
}

// Converts the compiler extended names into the old user defined name for the error output.
// No new memory is allocated.
static inline const char *get_pretty_name(const char *name)
{
    if (STRprefix("@fun_", name))
    {
        return name + 5; // remove the '@fun_' part
    }
    else if (STRprefix("@", name))
    {
        // Find var name of syntax @<prefix>_<varname>
        unsigned int start = 0;
        while (name[start] != '_' && name[start] != '\0')
        {
            start++;
        }
        start++; // Consume the '_' too

        release_assert(name[start] != '\0');
        return name + start;
    }
    else
    {
        return name;
    }
}

/// Addes a vardec after the last vardecs in the current funbdoy
/// and returns the new last vardecs.
static node_st *add_vardec(node_st *vardec, node_st *last_vardec, node_st *funbody)
{
    release_assert(NODE_TYPE(vardec) == NT_VARDEC);
    release_assert(last_vardec == NULL || NODE_TYPE(last_vardec) == NT_VARDECS);
    release_assert(NODE_TYPE(funbody) == NT_FUNBODY);

    if (last_vardec == NULL)
    {
        node_st *new_vardec = ASTvardecs(vardec, FUNBODY_VARDECS(funbody));
        FUNBODY_VARDECS(funbody) = new_vardec;
        return new_vardec;
    }
    else
    {
        node_st *new_vardec = ASTvardecs(vardec, VARDECS_NEXT(last_vardec));
        VARDECS_NEXT(last_vardec) = new_vardec;
        return new_vardec;
    }
}

/// Addes a statment after the last statments in the current funbdoy
/// and returns the new last statments
static node_st *add_stmt(node_st *stmt, node_st *last_stmts, node_st *funbody)
{
    release_assert(last_stmts == NULL || NODE_TYPE(last_stmts) == NT_STATEMENTS);
    release_assert(NODE_TYPE(funbody) == NT_FUNBODY);

    if (last_stmts == NULL)
    {
        node_st *new_stmts = ASTstatements(stmt, FUNBODY_STMTS(funbody));
        FUNBODY_STMTS(funbody) = new_stmts;
        return new_stmts;
    }
    else
    {
        node_st *new_stmts = ASTstatements(stmt, STATEMENTS_NEXT(last_stmts));
        STATEMENTS_NEXT(last_stmts) = new_stmts;
        return new_stmts;
    }
}

/// Finds the correct stmts position to insert the find_name assign in the __init function
static node_st *search_last_init_stmts(bool use_arrayexpr, node_st *stmts, node_st *decls,
                                       const char *find_name, node_st **out_decls)
{
    if (stmts == NULL)
    {
        return NULL;
    }

    node_st *last_stmts = NULL;
    while (decls != NULL)
    {
        node_st *decl = DECLARATIONS_DECL(decls);
        if (NODE_TYPE(decl) == NT_GLOBALDEF)
        {
            node_st *var = VARDEC_VAR(GLOBALDEF_VARDEC(decl));
            if (!use_arrayexpr && NODE_TYPE(var) == NT_ARRAYEXPR)
            {
                decls = DECLARATIONS_NEXT(decls);
                continue;
            }

            char *decl_name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYEXPR_VAR(var));
            if (STReq(decl_name, find_name) || stmts == NULL)
            {
                *out_decls = decls;
                return last_stmts;
            }

            node_st *stmt = STATEMENTS_STMT(stmts);
            if (NODE_TYPE(stmt) == NT_FORLOOP || NODE_TYPE(stmt) == NT_ARRAYASSIGN)
            {
                last_stmts = stmts;
                stmts = STATEMENTS_NEXT(stmts);
                continue;
            }

            char *stmt_name = VAR_NAME(ASSIGN_VAR(stmt));
            if (STReq(decl_name, stmt_name))
            {
                last_stmts = stmts;
                stmts = STATEMENTS_NEXT(stmts);
            }
        }
        decls = DECLARATIONS_NEXT(decls);
    }

    return NULL;
}
