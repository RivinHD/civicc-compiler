#include "ccngen/ast.h"
#include "code_gen_preparation/unpack_arrayinit.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdint.h>
#include <stdio.h>

static node_st *program_decls = NULL;
static node_st *last_fundef = NULL;
static htable_stptr current = NULL;
static uint32_t loop_counter = 0;
static node_st *last_vardecs = NULL;
static node_st *last_stmts = NULL;
static node_st *last_init_decls = NULL;

static void reset_state()
{
    program_decls = NULL;
    last_fundef = NULL;
    current = NULL;
    loop_counter = 0;
    last_vardecs = NULL;
    last_stmts = NULL;
    last_init_decls = NULL;
}

node_st *CGP_HAvardecs(node_st *node)
{
    TRAVopt(VARDECS_VARDEC(node));
    last_vardecs = node;
    TRAVopt(VARDECS_NEXT(node));
    return node;
}

node_st *CGP_HAvardec(node_st *node)
{
    node_st *funbody = FUNDEF_FUNBODY(last_fundef);
    release_assert(current != NULL);

    node_st *var = VARDEC_VAR(node);
    node_st *expr = VARDEC_EXPR(node);

    if (NODE_TYPE(var) == NT_ARRAYEXPR)
    {
        if (STReq(global_init_func, VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(last_fundef)))))
        {
            char *name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYEXPR_VAR(var));
            last_stmts = search_last_init_stmts(
                true, last_stmts == NULL ? FUNBODY_STMTS(funbody) : last_stmts,
                last_init_decls == NULL ? program_decls : last_init_decls, name, &last_init_decls);
        }

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
        last_vardecs = add_vardec(dims_vardec, last_vardecs, funbody);

        // We use an assignment for @loops_dims in __init function because we can have an
        // exported array that as a variable for its dimensions thus this variable gets
        // assigned in the __init function
        node_st *dims_assign = ASTassign(CCNcopy(dims_var), alloc_expr);
        last_stmts = add_stmt(dims_assign, last_stmts, funbody);

        // Step 1: Create allocation statement
        release_assert(funbody != NULL);
        node_st *alloc_var = ASTvar(STRcpy(alloc_func));
        node_st *alloc_exprs = ASTexprs(CCNcopy(dims_var), NULL);
        node_st *alloc_proccall = ASTproccall(alloc_var, alloc_exprs);
        node_st *alloc_stmt = ASTassign(CCNcopy(ARRAYEXPR_VAR(var)), alloc_proccall);
        last_stmts = add_stmt(alloc_stmt, last_stmts, funbody);

        // Extract expr to statement
        if (expr != NULL)
        {
            if (NODE_TYPE(expr) == NT_ARRAYINIT)
            {
                node_st *itop_stmts = NULL;
                node_st *ilast_stmts =
                    init_index_calculation(node, VAR_NAME(ARRAYEXPR_VAR(var)), &itop_stmts);
                if (ARRAYINIT_EXPR(expr) != NULL)
                {
                    release_assert(itop_stmts != NULL);
                    release_assert(ilast_stmts != NULL);
                    release_assert(STATEMENTS_NEXT(ilast_stmts) == NULL);

                    release_assert(last_stmts != NULL);
                    STATEMENTS_NEXT(ilast_stmts) = STATEMENTS_NEXT(last_stmts);
                    STATEMENTS_NEXT(last_stmts) = itop_stmts;
                    last_stmts = ilast_stmts;
                }
                else
                {
                    release_assert(itop_stmts == NULL);
                    release_assert(ilast_stmts == NULL);
                }
                CCNfree(expr);
            }
            else
            {
                node_st *loop_expression = ASTvar(STRfmt("@loop_expr%d", loop_counter));
                node_st *expr_assign = ASTassign(CCNcopy(loop_expression), expr);
                last_stmts = add_stmt(expr_assign, last_stmts, funbody);
                node_st *new_vardec = ASTvardec(loop_expression, NULL, VARDEC_TYPE(node));
                last_vardecs = add_vardec(new_vardec, last_vardecs, funbody);
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
                last_stmts = add_stmt(loop, last_stmts, funbody);
                node_st *new_loop_vardec = ASTvardec(CCNcopy(loop_var), NULL, DT_int);
                last_vardecs = add_vardec(new_loop_vardec, last_vardecs, funbody);
                success = HTinsert(current, VAR_NAME(VARDEC_VAR(new_loop_vardec)), new_loop_vardec);
                release_assert(success);
            }
        }

        loop_counter++;
    }
    else
    {
        if (expr != NULL)
        {
            node_st *assign = ASTassign(CCNcopy(VARDEC_VAR(node)), VARDEC_EXPR(node));
            VARDEC_EXPR(node) = NULL;
            last_stmts = add_stmt(assign, last_stmts, funbody);
        }
    }

    // Update node
    VARDEC_EXPR(node) = NULL;
    TRAVopt(VARDEC_VAR(node));

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
    node_st *parent_last_vardecs = last_vardecs;
    htable_stptr parent_current = current;
    node_st *parent_last_stmts = last_stmts;

    last_stmts = NULL;
    last_vardecs = NULL;
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
    last_vardecs = parent_last_vardecs;
    last_stmts = parent_last_stmts;
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
    program_decls = PROGRAM_DECLS(node);

    // Global arrays unpack assignment into the __init function
    node_st *entry = HTlookup(symbols, global_init_func);
    release_assert(entry != NULL);
    release_assert(NODE_TYPE(entry) == NT_FUNDEF);
    last_fundef = entry;
    current = FUNDEF_SYMBOLS(entry);

    TRAVopt(PROGRAM_DECLS(node));

    return node;
}
