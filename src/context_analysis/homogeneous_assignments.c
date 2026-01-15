#include "ccngen/ast.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>

static node_st *last_funbody = NULL;
static uint32_t loop_counter = 0;

node_st *CA_HAvardec(node_st* node)
{
    node_st *temp_var = VARDEC_VAR(node);
    if (NODE_TYPE(temp_var) == NT_ARRAYEXPR)
    {
        node_st *expr = VARDEC_EXPR(node);

        node_st* dim = ARRAYEXPR_DIMS(temp_var);
        node_st* alloc_expr = CCNcopy(EXPRS_EXPR(dim)); 
        dim = EXPRS_NEXT(dim);
        while (dim != NULL)
        {
            ASTbinop(alloc_expr, CCNcopy(EXPRS_EXPR(dim)), BO_mul);
            EXPRS_EXPR(dim);
            dim = EXPRS_NEXT(dim);
        }

        node_st* loop_var = ASTvar(STRfmt("@loop_var%d", loop_counter));
        node_st* loop_assign = ASTassign(loop_var, ASTint(0));
        node_st* loop_array_exprs = ASTexprs(CCNcopy(loop_var), NULL);
        node_st* loop_array_assign = ASTarrayexpr(loop_array_exprs, temp_var);
        node_st* loop_block_assign = ASTarrayassign(loop_array_assign, expr);
        node_st* loop_stmts = ASTstatements(loop_block_assign, NULL);
        node_st* loop = ASTforloop(loop_assign, CCNcopy(alloc_expr), NULL, loop_stmts);
        
        // alloc
        release_assert(last_funbody != NULL);
        node_st* alloc_var = ASTvar("@alloc");
        node_st* alloc_exprs = ASTexprs(dim, NULL);
        node_st* alloc_stmt = ASTproccall(alloc_var, alloc_exprs);

        node_st* stmts = FUNBODY_STMTS(last_funbody);
        node_st* assign_loop_stmts = ASTstatements(loop, stmts);
        node_st* alloc_stmts = ASTstatements(alloc_stmt, assign_loop_stmts);
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
