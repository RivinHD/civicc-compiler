#include "ccngen/ast.h"
#include "context_analysis/definitions.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "to_string.h"
#include "user_types.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdio.h>

static node_st *last_fundef = NULL;
static uint32_t loop_counter = 0;

// TODO skip all assign statement in last_funbody starting with '@' and only then add additional
// statements e.g. VAR_NAME(ASSIGN_VAR(<node>))[0] == '@' should be skipped
node_st *CA_HAvardec(node_st *node)
{
    node_st *cur_funbody = FUNDEF_FUNBODY(last_fundef);

    node_st *temp_var = VARDEC_VAR(node);
    if (NODE_TYPE(temp_var) == NT_ARRAYEXPR)
    {
        node_st *expr = VARDEC_EXPR(node);

        // Step 0: Count iteration length
        node_st *dim = ARRAYEXPR_DIMS(temp_var);
        node_st *alloc_expr = CCNcopy(EXPRS_EXPR(dim));
        dim = EXPRS_NEXT(dim);
        while (dim != NULL)
        {
            alloc_expr = ASTbinop(alloc_expr, CCNcopy(EXPRS_EXPR(dim)), BO_mul);
            dim = EXPRS_NEXT(dim);
        }

        // Step 1: Create allocation statement
        release_assert(cur_funbody != NULL);
        node_st *alloc_var = ASTvar(STRcpy(alloc_func));
        node_st *alloc_exprs = ASTexprs(alloc_expr, NULL);
        node_st *alloc_proccall = ASTproccall(alloc_var, alloc_exprs);
        node_st *alloc_stmt = ASTassign(CCNcopy(ARRAYEXPR_VAR(temp_var)), alloc_proccall);

        // Step 2: Find position to append to
        node_st *stmts = FUNBODY_STMTS(cur_funbody);
        node_st *last_stmts = NULL;
        while (NODE_TYPE(STATEMENTS_STMT(stmts)) == NT_ASSIGN &&
            VAR_NAME(ASSIGN_VAR(STATEMENTS_STMT(stmts))) != NULL &&
               VAR_NAME(ASSIGN_VAR(STATEMENTS_STMT(stmts)))[0] == '@')
        {
            last_stmts = stmts;
            stmts = STATEMENTS_NEXT(stmts);

        }

        node_st *alloc_stmts = NULL;
        if (expr != NULL)
        {
            // Step 3.1: Create for Loop
            node_st *loop_var = ASTvar(STRfmt("@loop_var%d", loop_counter));
            node_st *loop_assign = ASTassign(loop_var, ASTint(0));
            node_st *loop_array_exprs = ASTexprs(CCNcopy(loop_var), NULL);
            node_st *loop_array_assign =
                ASTarrayexpr(loop_array_exprs, CCNcopy(ARRAYEXPR_VAR(temp_var)));
            node_st *loop_block_assign = ASTarrayassign(loop_array_assign, expr);
            node_st *loop_stmts = ASTstatements(loop_block_assign, NULL);
            node_st *loop = ASTforloop(loop_assign, CCNcopy(alloc_expr), NULL, loop_stmts);

            // Step 3.2: Append alloc + loop statement at the calculated position
            node_st *assign_loop_stmts = ASTstatements(loop, stmts);
            alloc_stmts = ASTstatements(alloc_stmt, assign_loop_stmts);
        }
        else
        {
            alloc_stmts = ASTstatements(alloc_stmt, stmts);
        }
        // @asd = 1;
        // ---
        // # my stuff
        // if ...

        release_assert(alloc_stmts != NULL);
        if (last_stmts != NULL)
        {
            STATEMENTS_NEXT(last_stmts) = alloc_stmts;
        }
        else 
        {
            FUNBODY_STMTS(cur_funbody) = alloc_stmts;
        }

        // Update node
        VARDEC_EXPR(node) = NULL;
    }

    TRAVopt(VARDEC_VAR(node));
    TRAVopt(VARDEC_EXPR(node));

    return node;
}

/**
 * Position to append nodes to.
 */
node_st *CA_HAfundef(node_st *node)
{
    last_fundef = node;
    loop_counter = 0;

    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));
    return node;
}

/**
 * For symbol table.
 */
node_st *CA_HAprogram(node_st *node)
{
    char *str = node_to_string(node);
    printf("%s\n", str);
    free(str);

    htable_stptr symbols = PROGRAM_SYMBOLS(node);

    // Global arrays unpack assignment into the __init function
    node_st *entry = HTlookup(symbols, global_init_func);
    release_assert(entry != NULL);
    release_assert(NODE_TYPE(entry) == NT_FUNDEF);
    last_fundef = entry;

    TRAVopt(PROGRAM_DECLS(node));

    printf("------------------------------");
    str = node_to_string(node);
    printf("%s\n", str);
    free(str);
    return node;
}
