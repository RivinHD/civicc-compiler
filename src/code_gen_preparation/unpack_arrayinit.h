#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "palm/str.h"
#include "release_assert.h"
#include <ccn/dynamic_core.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Traverse the nested Array inits and genereate statements
static node_st *nested_init_index_calculation(node_st *node, node_st *start_exprs,
                                              const char *var_name, node_st **out_top_stmts)
{
    release_assert(node != NULL);
    release_assert(start_exprs != NULL);
    release_assert(NODE_TYPE(start_exprs) == NT_EXPRS);

    if (NODE_TYPE(node) != NT_ARRAYINIT)
    {
        // Here: rec_init = Int -- val:'1'
        node_st *new_var = ASTvar(STRcpy(var_name));
        node_st *new_arrayexpr = ASTarrayexpr(start_exprs, new_var);
        node_st *new_arrayassign = ASTarrayassign(new_arrayexpr, CCNcopy(node));

        node_st *new_stmts = ASTstatements(new_arrayassign, NULL);
        *out_top_stmts = new_stmts;
        return new_stmts;
    }

    int index_counter = 0;
    node_st *last_stmts = NULL;
    while (node != NULL)
    {
        release_assert(NODE_TYPE(node) == NT_ARRAYINIT);
        if (ARRAYINIT_EXPR(node) != NULL)
        {
            node_st *copy_start_exprs = CCNcopy(start_exprs);
            release_assert(NODE_TYPE(copy_start_exprs) == NT_EXPRS);
            node_st *exprs = copy_start_exprs;
            while (EXPRS_NEXT(exprs) != NULL)
            {
                exprs = EXPRS_NEXT(exprs);
                release_assert(NODE_TYPE(exprs) == NT_EXPRS);
            }

            node_st *new_exprs = ASTexprs(ASTint(index_counter++), NULL);
            EXPRS_NEXT(exprs) = new_exprs;
            node_st *top_stmts = NULL;
            node_st *stmts = nested_init_index_calculation(ARRAYINIT_EXPR(node), copy_start_exprs,
                                                           var_name, &top_stmts);
            release_assert(stmts != NULL);
            release_assert(top_stmts != NULL);

            if (last_stmts == NULL)
            {
                last_stmts = stmts;
                *out_top_stmts = top_stmts;
            }
            else
            {
                release_assert(STATEMENTS_NEXT(last_stmts) == NULL);
                STATEMENTS_NEXT(last_stmts) = top_stmts;
                last_stmts = stmts;
            }
        }
        node = ARRAYINIT_NEXT(node);
    }
    CCNfree(start_exprs);
    return last_stmts;
}

// Unpack the array init into multiple statements.
static node_st *init_index_calculation(node_st *node, const char *var_name, node_st **out_top_stmts)
{
    node_st *rec_init = VARDEC_EXPR(node);
    int index_counter = 0;
    node_st *last_stmts = NULL;
    while (rec_init != NULL)
    {
        if (ARRAYINIT_EXPR(rec_init) != NULL)
        {
            node_st *new_exprs = ASTexprs(ASTint(index_counter++), NULL);
            node_st *top_stmts = NULL;
            node_st *stmts = nested_init_index_calculation(ARRAYINIT_EXPR(rec_init), new_exprs,
                                                           var_name, &top_stmts);
            release_assert(stmts != NULL);
            release_assert(top_stmts != NULL);

            if (last_stmts == NULL)
            {
                last_stmts = stmts;
                *out_top_stmts = top_stmts;
            }
            else
            {
                release_assert(STATEMENTS_NEXT(last_stmts) == NULL);
                STATEMENTS_NEXT(last_stmts) = top_stmts;
                last_stmts = stmts;
            }
        }
        rec_init = ARRAYINIT_NEXT(rec_init);
    }
    return last_stmts;
}
