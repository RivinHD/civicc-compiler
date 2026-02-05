#include "ccngen/ast.h"
#include "release_assert.h"
#include "utils.h"

#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>

static htable_stptr current = NULL;

static void reset_state()
{
    current = NULL;
}

node_st *CGP_DRarrayexpr(node_st *node)
{
    node_st *expr = ARRAYEXPR_DIMS(node);
    node_st *start_expr = EXPRS_EXPR(expr);
    node_st *entry = deep_lookup(current, VAR_NAME(ARRAYEXPR_VAR(node)));
    release_assert(entry != NULL);
    node_st *arrdef = NULL;
    switch (NODE_TYPE(entry))
    {
    case NT_VARDEC:
        arrdef = VARDEC_VAR(entry);
        break;
    case NT_PARAMS:
        arrdef = PARAMS_VAR(entry);
        break;
    case NT_GLOBALDEC:
        arrdef = GLOBALDEC_VAR(entry);
        break;
    default:
        release_assert(false);
        break;
    }

    expr = EXPRS_NEXT(expr);
    if (expr == NULL || node == arrdef)
    {
        // Already one dimensional no need to flatten
        // Or node is a definition located in the symbol table, no flattening needed.
        return node;
    }

    if (NODE_TYPE(arrdef) == NT_ARRAYVAR)
    {
        node_st *dim = ARRAYVAR_DIMS(arrdef);
        dim = DIMENSIONVARS_NEXT(dim); // Skip the first dim, as not relevant for the stride
        while (expr != NULL && dim != NULL)
        {
            start_expr =
                ASTbinop(ASTbinop(start_expr, CCNcopy(DIMENSIONVARS_DIM(dim)), BO_mul, DT_int),
                         EXPRS_EXPR(expr), BO_add, DT_int);
            EXPRS_EXPR(expr) = NULL;
            expr = EXPRS_NEXT(expr);
            dim = DIMENSIONVARS_NEXT(dim);
        }

        release_assert(expr == NULL);
        release_assert(dim == NULL);
    }
    else if (NODE_TYPE(arrdef) == NT_ARRAYEXPR)
    {
        node_st *dim = ARRAYEXPR_DIMS(arrdef);
        dim = EXPRS_NEXT(dim); // Skip the first dim, as not relevant for the stride
        while (expr != NULL && dim != NULL)
        {
            start_expr = ASTbinop(ASTbinop(start_expr, CCNcopy(EXPRS_EXPR(dim)), BO_mul, DT_int),
                                  EXPRS_EXPR(expr), BO_add, DT_int);
            EXPRS_EXPR(expr) = NULL;
            expr = EXPRS_NEXT(expr);
            dim = EXPRS_NEXT(dim);
        }

        release_assert(expr == NULL);
        release_assert(dim == NULL);
    }
    else
    {
        release_assert(false);
    }

    CCNfree(EXPRS_NEXT(ARRAYEXPR_DIMS(node)));
    EXPRS_EXPR(ARRAYEXPR_DIMS(node)) = start_expr;
    EXPRS_NEXT(ARRAYEXPR_DIMS(node)) = NULL;

    return node;
}

node_st *CGP_DRfundef(node_st *node)
{
    htable_stptr parent_current = current;
    current = FUNDEF_SYMBOLS(node);
    release_assert(current != NULL);

    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));

    current = parent_current;
    release_assert(current != NULL);

    return node;
}

node_st *CGP_DRprogram(node_st *node)
{
    reset_state();
    current = PROGRAM_SYMBOLS(node);
    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
