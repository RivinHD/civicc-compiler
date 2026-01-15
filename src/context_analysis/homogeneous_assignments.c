#include "ccngen/ast.h"
#include "context_analysis/definitions.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdio.h>

static node_st *last_funbody = NULL;
static uint32_t loop_counter = 0;

node_st *CA_HAvardec(node_st *node)
{
    node_st *temp_var = VARDEC_VAR(node);
    if (NODE_TYPE(temp_var) == NT_ARRAYEXPR)
    {
        node_st *expr = VARDEC_EXPR(node);

        node_st *dim = ARRAYEXPR_DIMS(temp_var);
        node_st *alloc_expr = CCNcopy(EXPRS_EXPR(dim));
        dim = EXPRS_NEXT(dim);
        while (dim != NULL)
        {
            ASTbinop(alloc_expr, CCNcopy(EXPRS_EXPR(dim)), BO_mul);
            EXPRS_EXPR(dim);
            dim = EXPRS_NEXT(dim);
        }

        // alloc
        release_assert(last_funbody != NULL);
        node_st *alloc_var = ASTvar(STRcpy(alloc_func));
        node_st *alloc_exprs = ASTexprs(alloc_expr, NULL);
        node_st *alloc_stmt = ASTproccall(alloc_var, alloc_exprs);

        node_st *stmts = FUNBODY_STMTS(last_funbody);
        node_st *alloc_stmts = NULL;
        if (expr != NULL)
        {
            node_st *loop_var = ASTvar(STRfmt("@loop_var%d", loop_counter));
            node_st *loop_assign = ASTassign(loop_var, ASTint(0));
            node_st *loop_array_exprs = ASTexprs(CCNcopy(loop_var), NULL);
            node_st *loop_array_assign = ASTarrayexpr(loop_array_exprs, temp_var);
            node_st *loop_block_assign = ASTarrayassign(loop_array_assign, expr);
            node_st *loop_stmts = ASTstatements(loop_block_assign, NULL);
            node_st *loop = ASTforloop(loop_assign, CCNcopy(alloc_expr), NULL, loop_stmts);

            node_st *assign_loop_stmts = ASTstatements(loop, stmts);
            alloc_stmts = ASTstatements(alloc_stmt, assign_loop_stmts);
        }
        else
        {
            alloc_stmts = ASTstatements(alloc_stmt, stmts);
        }

        release_assert(alloc_stmts != NULL);
        FUNBODY_STMTS(last_funbody) = alloc_stmts;

        // Update node
        VARDEC_EXPR(node) = NULL;
    }
    return node;
}

/**
 * Place to store extracted expr
 */
node_st *CA_HAfunbody(node_st *node)
{
    last_funbody = node;
    loop_counter = 0;
    return node;
}

/**
 * For symbol table.
 */
node_st *CA_HAprogram(node_st *node)
{
    htable_stptr symbols = PROGRAM_SYMBOLS(node);

    // Global arrays unpack assignment into the __init function
    node_st *entry = HTlookup(symbols, global_init_func);
    release_assert(entry != NULL);
    release_assert(NODE_TYPE(entry) == NT_FUNDEF);
    last_funbody = FUNDEF_FUNBODY(entry);

    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
