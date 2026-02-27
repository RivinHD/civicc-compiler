#pragma once

#include "ccngen/enum.h"
#include "definitions.h"
#include "palm/ctinfo.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include <ccn/phase_driver.h>
#include <ccngen/ast.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <stdarg.h>
#include <stdio.h>
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

#define error_cache_size 1024 * 5 // 5 KiB
static char cache_file[error_cache_size];

static char *get_file_line(const char *filename, int start_line, int end_line)
{
    if (filename == NULL)
    {
        return NULL;
    }

    FILE *fd = fopen(filename, "r");
    if (fd == NULL)
    {
        return NULL;
    }

    int line_to_read = end_line - start_line + 1;

    while (start_line-- > 1) // Skip the start_lines
    {
        int c;
        while ((c = fgetc(fd)) != (int)'\n')
        {
            if (c == EOF)
            {
                return NULL;
            }
        }
    }

    int read_chars = 0;
    while (line_to_read-- > 0)
    {
        int c;
        while ((c = fgetc(fd)) != (int)'\n')
        {
            read_chars++;
            if (read_chars < error_cache_size)
            {
                cache_file[read_chars - 1] = (char)c;
            }
            else
            {
                // cache is full, do not read any further lines
                cache_file[read_chars - 1] = '\0';
                return cache_file;
            }

            if (c == EOF)
            {
                cache_file[read_chars - 1] = '\0';
                return cache_file;
            }
        }
        cache_file[read_chars++] = '\n';
    }

    // Replace last \n with \0
    cache_file[read_chars - 1] = '\0';
    return cache_file;
}

#define NODE_TO_CTINFO(node)                                                                       \
    {                                                                                              \
        (int)NODE_BLINE((node)),                                                                   \
        (int)NODE_BCOL((node)),                                                                    \
        (int)NODE_ELINE((node)),                                                                   \
        (int)NODE_ECOL((node)),                                                                    \
        NODE_FILENAME((node)),                                                                     \
        get_file_line(NODE_FILENAME((node)), (int)NODE_BLINE((node)), (int)NODE_ELINE((node))),    \
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
    CTIobj(CTI_ERROR, true, info, "'%s' already defined at %d:%d - %d:%d.", name, NODE_BLINE(found),
           NODE_BCOL(found), NODE_ELINE(found), NODE_ECOL(found));
}

static inline void error_invalid_identifier_name(node_st *node, node_st *found, const char *name)
{
    struct ctinfo info = NODE_TO_CTINFO(node);
    CTIobj(CTI_ERROR, true, info,
           "'%s' is not allowed to start with '_'. Defined at %d:%d - %d:%d.", name,
           NODE_BLINE(found), NODE_BCOL(found), NODE_ELINE(found), NODE_ECOL(found));
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

enum sideeffect
{
    // We will use logical OR the get the higest level
    SEFF_NULL = 0,
    SEFF_NO = 1,         // 0b1
    SEFF_PROCESSING = 3, // 0b11
    SEFF_YES = 7,        // 0b111, should always be the max value
};

static enum sideeffect check_fun_sideeffect(node_st *fun, htable_stptr sideeffect_table);

// Checks if a expression has side effects and keep a table of already check fundef and fundec.
static enum sideeffect check_expr_sideeffect(node_st *expr, htable_stptr symbols,
                                             htable_stptr sideeffect_table)
{
    release_assert(expr != NULL);
    release_assert(symbols != NULL);
    release_assert(sideeffect_table != NULL);

    enum sideeffect effect = SEFF_NO;
    switch (NODE_TYPE(expr))
    {
    case NT_PROCCALL: {
        char *name = VAR_NAME(PROCCALL_VAR(expr));
        if (STReq(name, alloc_func))
        {
            return SEFF_NO;
        }

        node_st *entry = deep_lookup(symbols, name);
        release_assert(entry != NULL);
        release_assert(NODE_TYPE(entry) == NT_FUNDEF || NODE_TYPE(entry) == NT_FUNDEC);
        if (NODE_TYPE(entry) == NT_FUNDEC)
        {
            return SEFF_YES;
        }

        void *fun_eff = HTlookup(sideeffect_table, entry);
        if (fun_eff == NULL)
        {
            enum sideeffect eff = check_fun_sideeffect(entry, sideeffect_table);
            if (eff == SEFF_PROCESSING)
            {
                // We could not fully check this function, because it has cycly
                // dependecies. Thus we can not ensure its value in the side effect table is
                // correct.
                HTremove(sideeffect_table, entry);
                effect |= SEFF_NO; // No other statement except processing proccalls have side
                                   // effects the already processsing proccall dependecies will be
                                   // check further up in the recursion
            }
            else
            {
                effect |= eff;
            }
        }
        else if (fun_eff == (void *)SEFF_PROCESSING && FUNDEF_SYMBOLS(entry) == symbols)
        {
            // Recursion to itself found.
            // The rest is already check by an already called function, no need to check again.
            return SEFF_NO;
        }
        else
        {
            ptrdiff_t value = (ptrdiff_t)fun_eff;
            release_assert(value >= SEFF_NULL);
            release_assert(value <= SEFF_YES);
            enum sideeffect eff = (enum sideeffect)value;
            release_assert(eff != SEFF_NULL);
            effect |= eff;
        }
    }
    break;
    case NT_TERNARY:
        release_assert(TERNARY_PRED(expr) != NULL);
        release_assert(TERNARY_PFALSE(expr) != NULL);
        release_assert(TERNARY_PTRUE(expr) != NULL);
        effect |= check_expr_sideeffect(TERNARY_PRED(expr), symbols, sideeffect_table);
        effect |= check_expr_sideeffect(TERNARY_PFALSE(expr), symbols, sideeffect_table);
        effect |= check_expr_sideeffect(TERNARY_PTRUE(expr), symbols, sideeffect_table);
        break;
    case NT_BINOP:
        release_assert(BINOP_LEFT(expr) != NULL);
        release_assert(BINOP_RIGHT(expr) != NULL);
        effect |= check_expr_sideeffect(BINOP_LEFT(expr), symbols, sideeffect_table);
        effect |= check_expr_sideeffect(BINOP_RIGHT(expr), symbols, sideeffect_table);
        break;
    case NT_MONOP:
        release_assert(MONOP_LEFT(expr) != NULL);
        effect |= check_expr_sideeffect(MONOP_LEFT(expr), symbols, sideeffect_table);
        break;
    case NT_ARRAYEXPR: {
        node_st *exprs = ARRAYEXPR_DIMS(expr);
        while (exprs != NULL)
        {
            release_assert(EXPRS_EXPR(exprs) != NULL);
            effect |= check_expr_sideeffect(EXPRS_EXPR(exprs), symbols, sideeffect_table);
            if (effect == SEFF_YES)
            {
                break;
            }
            exprs = EXPRS_NEXT(exprs);
        }
    }
    break;

    case NT_CAST:
        release_assert(CAST_EXPR(expr) != NULL);
        effect |= check_expr_sideeffect(CAST_EXPR(expr), symbols, sideeffect_table);
        break;
    case NT_POP:
        release_assert(POP_EXPR(expr) != NULL);
        effect |= check_expr_sideeffect(POP_EXPR(expr), symbols, sideeffect_table);
        if (POP_REPLACE(expr) != NULL)
        {
            release_assert(POP_EXPR(expr) != NULL);
            effect |= check_expr_sideeffect(POP_EXPR(expr), symbols, sideeffect_table);
        }
        break;
    case NT_VAR:
    case NT_INT:
    case NT_FLOAT:
    case NT_BOOL:
        // Not possible to have any side effects and dont have children that can have sideeffects.
        break;
    default:
        release_assert(false);
        break;
    }

    return effect;
}

// Checks if a statment has side effects and keep a table of already check fundef and fundec.
static enum sideeffect check_stmt_sideeffect(node_st *stmt, htable_stptr symbols,
                                             htable_stptr sideeffect_table)
{
    release_assert(stmt != NULL);
    release_assert(symbols != NULL);
    release_assert(sideeffect_table != NULL);

    node_st *stmts = NULL;
    enum sideeffect effect = SEFF_NO;
    switch (NODE_TYPE(stmt))
    {
    case NT_ASSIGN:
        release_assert(ASSIGN_EXPR(stmt) != NULL);
        effect |= check_expr_sideeffect(ASSIGN_EXPR(stmt), symbols, sideeffect_table);
        {
            node_st *var = ASSIGN_VAR(stmt);
            char *name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYEXPR_VAR(var));
            node_st *entry = HTlookup(symbols, name);
            if (entry == NULL)
            { // assignment to none local variable
                effect = SEFF_YES;
            }
        }
        break;
    case NT_PROCCALL:
        release_assert(stmt != NULL);
        effect |= check_expr_sideeffect(stmt, symbols, sideeffect_table);
        break;
    case NT_IFSTATEMENT:
        release_assert(IFSTATEMENT_EXPR(stmt) != NULL);
        effect |= check_expr_sideeffect(IFSTATEMENT_EXPR(stmt), symbols, sideeffect_table);
        stmts = IFSTATEMENT_BLOCK(stmt);
        while (stmts != NULL)
        {
            release_assert(STATEMENTS_STMT(stmts) != NULL);
            effect |= check_stmt_sideeffect(STATEMENTS_STMT(stmts), symbols, sideeffect_table);
            if (effect == SEFF_YES)
            {
                break;
            }
            stmts = STATEMENTS_NEXT(stmts);
        }
        stmts = IFSTATEMENT_ELSE_BLOCK(stmt);
        while (stmts != NULL)
        {
            release_assert(STATEMENTS_STMT(stmts) != NULL);
            effect |= check_stmt_sideeffect(STATEMENTS_STMT(stmts), symbols, sideeffect_table);
            if (effect == SEFF_YES)
            {
                break;
            }
            stmts = STATEMENTS_NEXT(stmts);
        }
        break;
    case NT_WHILELOOP:
        release_assert(WHILELOOP_EXPR(stmt) != NULL);
        effect |= check_expr_sideeffect(WHILELOOP_EXPR(stmt), symbols, sideeffect_table);
        stmts = WHILELOOP_BLOCK(stmt);
        while (stmts != NULL)
        {
            effect |= check_stmt_sideeffect(STATEMENTS_STMT(stmts), symbols, sideeffect_table);
            if (effect == SEFF_YES)
            {
                break;
            }
            stmts = STATEMENTS_NEXT(stmts);
        }
        break;
    case NT_DOWHILELOOP:
        release_assert(DOWHILELOOP_EXPR(stmt) != NULL);
        effect |= check_expr_sideeffect(DOWHILELOOP_EXPR(stmt), symbols, sideeffect_table);
        stmts = DOWHILELOOP_BLOCK(stmt);
        while (stmts != NULL)
        {
            effect |= check_stmt_sideeffect(STATEMENTS_STMT(stmts), symbols, sideeffect_table);
            if (effect == SEFF_YES)
            {
                break;
            }
            stmts = STATEMENTS_NEXT(stmts);
        }
        break;
    case NT_FORLOOP:
        release_assert(FORLOOP_ASSIGN(stmt) != NULL);
        release_assert(ASSIGN_EXPR(FORLOOP_ASSIGN(stmt)) != NULL);
        release_assert(FORLOOP_COND(stmt) != NULL);
        effect |=
            check_expr_sideeffect(ASSIGN_EXPR(FORLOOP_ASSIGN(stmt)), symbols, sideeffect_table);
        effect |= check_expr_sideeffect(FORLOOP_COND(stmt), symbols, sideeffect_table);
        effect |= FORLOOP_ITER(stmt) == NULL // Can be null
                      ? SEFF_NO
                      : check_expr_sideeffect(FORLOOP_ITER(stmt), symbols, sideeffect_table);
        stmts = FORLOOP_BLOCK(stmt);
        while (stmts != NULL)
        {
            release_assert(STATEMENTS_STMT(stmts) != NULL);
            effect |= check_stmt_sideeffect(STATEMENTS_STMT(stmts), symbols, sideeffect_table);
            if (effect == SEFF_YES)
            {
                break;
            }
            stmts = STATEMENTS_NEXT(stmts);
        }
        break;
    case NT_RETSTATEMENT:
        effect |= RETSTATEMENT_EXPR(stmt) == NULL
                      ? SEFF_NO
                      : check_expr_sideeffect(RETSTATEMENT_EXPR(stmt), symbols, sideeffect_table);
        break;
    case NT_ARRAYASSIGN:
        release_assert(ARRAYASSIGN_EXPR(stmt) != NULL);
        effect |= check_expr_sideeffect(ARRAYASSIGN_EXPR(stmt), symbols, sideeffect_table);
        break;
    default:
        release_assert(false);
        break;
    }

    return effect;
}

// Checks if a fundec/fundef has side effects and keep a table of already check fundef and fundec.
static enum sideeffect check_fun_sideeffect(node_st *fun, htable_stptr sideeffect_table)
{
    release_assert(fun != NULL);
    release_assert(sideeffect_table != NULL);
    release_assert(NODE_TYPE(fun) == NT_FUNDEF || NODE_TYPE(fun) == NT_FUNDEC);

    if (NODE_TYPE(fun) == NT_FUNDEC)
    {
        // Can not check if extern functions have side effects, thus we need to assume yes.
        return SEFF_YES;
    }

    char *name = VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(fun)));
    if (STReq(name, alloc_func))
    {
        return SEFF_NO;
    }

    void *entry = HTlookup(sideeffect_table, fun);
    if (entry == NULL)
    {
        release_assert(NODE_TYPE(fun) == NT_FUNDEF);
        bool success = HTinsert(sideeffect_table, fun, (void *)SEFF_PROCESSING);
        release_assert(success);

        enum sideeffect effect = SEFF_NO;
        node_st *stmts = FUNBODY_STMTS(FUNDEF_FUNBODY(fun));
        while (stmts != NULL)
        {
            node_st *stmt = STATEMENTS_STMT(stmts);
            effect |= check_stmt_sideeffect(stmt, FUNDEF_SYMBOLS(fun), sideeffect_table);
            if (effect == SEFF_YES)
            {
                break;
            }
            stmts = STATEMENTS_NEXT(stmts);
        }

        HTremove(sideeffect_table, fun);
        HTinsert(sideeffect_table, fun, (void *)effect);
        entry = (void *)effect;
    }

    ptrdiff_t value = (ptrdiff_t)entry;
    release_assert(value != SEFF_NULL);
    release_assert(value <= SEFF_YES);
    return (enum sideeffect)value;
}

static void free_symbols(htable_stptr symbols)
{
    for (htable_iter_st *iter = HTiterate(symbols); iter; iter = HTiterateNext(iter))
    {
        char *key = HTiterKey(iter);
        if (STRprefix("@fun_", key))
        {
            // Free function keys
            free(key);
        }
    }

    HTdelete(symbols);
}

static void check_phase_error()
{
    if (CTIgetErrors() > 0)
    {
        CCNerrorPhase();
    }
}
