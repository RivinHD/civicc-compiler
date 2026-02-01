#include "ccngen/ast.h"
#include "code_gen_preparation/unpack_arrayinit.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdint.h>
#include <stdio.h>

static node_st *first_decls = NULL;
static node_st *last_fundef = NULL;
static htable_stptr current = NULL;
static uint32_t loop_counter = 0;

static void reset_state()
{
    first_decls = NULL;
    last_fundef = NULL;
    current = NULL;
    loop_counter = 0;
}

node_st *CGP_HAvardec(node_st *node)
{
    node_st *funbody = FUNDEF_FUNBODY(last_fundef);
    release_assert(current != NULL);

    node_st *var = VARDEC_VAR(node);
    if (NODE_TYPE(var) == NT_ARRAYEXPR)
    {
        node_st *expr = VARDEC_EXPR(node);

        // Step 0: Count iteration length
        node_st *dim = ARRAYEXPR_DIMS(var);
        node_st *alloc_expr = CCNcopy(EXPRS_EXPR(dim));
        dim = EXPRS_NEXT(dim);
        while (dim != NULL)
        {
            alloc_expr = ASTbinop(alloc_expr, CCNcopy(EXPRS_EXPR(dim)), BO_mul, DT_int);
            dim = EXPRS_NEXT(dim);
        }

        // Step 0: Create temp dimension size var
        node_st *dims_var = ASTvar(STRfmt("@loop_dims%d", loop_counter));
        node_st *dims_vardec = ASTvardec(dims_var, NULL, DT_int);
        bool success = HTinsert(current, VAR_NAME(VARDEC_VAR(dims_vardec)), dims_vardec);
        release_assert(success);
        node_st *dims_new_vardecs = ASTvardecs(dims_vardec, FUNBODY_VARDECS(funbody));
        FUNBODY_VARDECS(funbody) = dims_new_vardecs;
        node_st *dims_assign = ASTassign(CCNcopy(dims_var), alloc_expr);

        // Step 1: Create allocation statement
        release_assert(funbody != NULL);
        node_st *alloc_var = ASTvar(STRcpy(alloc_func));
        node_st *alloc_exprs = ASTexprs(CCNcopy(dims_var), NULL);
        node_st *alloc_proccall = ASTproccall(alloc_var, alloc_exprs);
        node_st *alloc_stmt = ASTassign(CCNcopy(ARRAYEXPR_VAR(var)), alloc_proccall);

        // Step 2: Find position to append to
        node_st *stmts = FUNBODY_STMTS(funbody);
        node_st *last_stmts = NULL;
        while (stmts != NULL && NODE_TYPE(STATEMENTS_STMT(stmts)) == NT_ASSIGN &&
               VAR_NAME(ASSIGN_VAR(STATEMENTS_STMT(stmts))) != NULL &&
               VAR_NAME(ASSIGN_VAR(STATEMENTS_STMT(stmts)))[0] == '@')
        {
            last_stmts = stmts;
            stmts = STATEMENTS_NEXT(stmts);
        }

        if (expr != NULL)
        {
            if (NODE_TYPE(expr) == NT_ARRAYINIT)
            {
                node_st *top_stmts = NULL;
                node_st *last_stmts =
                    init_index_calculation(node, VAR_NAME(ARRAYEXPR_VAR(var)), &top_stmts);
                if (ARRAYINIT_EXPR(expr) != NULL)
                {
                    release_assert(top_stmts != NULL);
                    release_assert(last_stmts != NULL);
                    release_assert(STATEMENTS_NEXT(last_stmts) == NULL);

                    STATEMENTS_NEXT(last_stmts) = stmts;
                    stmts = top_stmts;
                }
                else
                {
                    release_assert(top_stmts == NULL);
                    release_assert(last_stmts == NULL);
                }
                CCNfree(expr);
            }
            else
            {
                node_st *loop_expression = ASTvar(STRfmt("@loop_expr%d", loop_counter));
                node_st *expr_assign = ASTassign(CCNcopy(loop_expression), expr);
                node_st *new_vardec = ASTvardec(loop_expression, NULL, VARDEC_TYPE(node));
                bool success = HTinsert(current, VAR_NAME(VARDEC_VAR(new_vardec)), new_vardec);
                release_assert(success);

                // Step 3.1: Create for Loop
                node_st *loop_var = ASTvar(STRfmt("@loop_var%d", loop_counter));
                node_st *loop_assign = ASTassign(loop_var, ASTint(0));
                node_st *loop_array_exprs = ASTexprs(CCNcopy(loop_var), NULL);
                node_st *loop_array_assign =
                    ASTarrayexpr(loop_array_exprs, CCNcopy(ARRAYEXPR_VAR(var)));
                node_st *loop_block_assign =
                    ASTarrayassign(loop_array_assign, CCNcopy(loop_expression));
                node_st *loop_stmts = ASTstatements(loop_block_assign, NULL);
                node_st *loop = ASTforloop(loop_assign, CCNcopy(dims_var), NULL, loop_stmts);
                node_st *new_loop_vardec = ASTvardec(CCNcopy(loop_var), NULL, DT_int);
                success = HTinsert(current, VAR_NAME(VARDEC_VAR(new_loop_vardec)), new_loop_vardec);
                release_assert(success);
                node_st *new_loop_vardecs = ASTvardecs(new_loop_vardec, FUNBODY_VARDECS(funbody));
                FUNBODY_VARDECS(funbody) = new_loop_vardecs;

                // Step 3.2: Append alloc + loop statement at the calculated position
                stmts = ASTstatements(loop, stmts);
                stmts = ASTstatements(expr_assign, stmts);

                node_st *new_vardecs = ASTvardecs(new_vardec, FUNBODY_VARDECS(funbody));
                FUNBODY_VARDECS(funbody) = new_vardecs;
            }
        }

        stmts = ASTstatements(alloc_stmt, stmts);
        stmts = ASTstatements(dims_assign, stmts);

        loop_counter++;
        release_assert(stmts != NULL);
        if (last_stmts != NULL)
        {
            STATEMENTS_NEXT(last_stmts) = stmts;
        }
        else
        {
            FUNBODY_STMTS(funbody) = stmts;
        }

        // Update node
        VARDEC_EXPR(node) = NULL;
    }

    TRAVopt(VARDEC_VAR(node));
    TRAVopt(VARDEC_EXPR(node));

    return node;
}

node_st *CGP_HAfunbody(node_st *node)
{
    TRAVopt(FUNBODY_VARDECS(node));
    TRAVopt(FUNBODY_LOCALFUNDEFS(node));
    TRAVopt(FUNBODY_STMTS(node));

    return node;
}

/**
 * Position to append nodes to.
 */
node_st *CGP_HAfundef(node_st *node)
{
    node_st *parent_fundef = last_fundef;
    uint32_t parent_loopcounter = loop_counter;

    htable_stptr parent_current = current;
    current = FUNDEF_SYMBOLS(node);
    release_assert(current != NULL);

    last_fundef = node;
    loop_counter = 0;

    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));

    current = parent_current;
    release_assert(current != NULL);

    loop_counter = parent_loopcounter;
    last_fundef = parent_fundef;
    return node;
}

/**
 * For symbol table.
 */
node_st *CGP_HAprogram(node_st *node)
{
    reset_state();
    htable_stptr symbols = PROGRAM_SYMBOLS(node);
    current = symbols;
    first_decls = PROGRAM_DECLS(node);

    // Global arrays unpack assignment into the __init function
    node_st *entry = HTlookup(symbols, global_init_func);
    release_assert(entry != NULL);
    release_assert(NODE_TYPE(entry) == NT_FUNDEF);
    last_fundef = entry;
    current = FUNDEF_SYMBOLS(entry);

    TRAVopt(PROGRAM_DECLS(node));

    PROGRAM_DECLS(node) = first_decls;

    return node;
}
