#include "ccngen/ast.h"
#include "release_assert.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <stdio.h>

node_st *OPT_CFbinop(node_st *node)
{
    BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
    BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));

    if (NODE_TYPE(BINOP_LEFT(node)) == NT_INT && NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
    {
        int result;

        switch (BINOP_OP(node))
        {
        case BO_NULL:
            release_assert(false);
            break;
        case BO_add:
            result = INT_VAL(BINOP_LEFT(node)) + INT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTint(result);
            break;
        case BO_sub:
            result = INT_VAL(BINOP_LEFT(node)) - INT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTint(result);
            break;
        case BO_mul:
            result = INT_VAL(BINOP_LEFT(node)) * INT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTint(result);
            break;
        case BO_div:
            result = INT_VAL(BINOP_LEFT(node)) / INT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTint(result);
            break;
        case BO_mod:
            result = INT_VAL(BINOP_LEFT(node)) % INT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTint(result);
            break;
        }
    }
    else if (NODE_TYPE(BINOP_LEFT(node)) == NT_FLOAT && NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
    {
        double result;

        switch (BINOP_OP(node))
        {
        case BO_NULL:
            release_assert(false);
            break;
        case BO_add:
            result = FLOAT_VAL(BINOP_LEFT(node)) + FLOAT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTfloat(result);
            break;
        case BO_sub:
            result = FLOAT_VAL(BINOP_LEFT(node)) - FLOAT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTfloat(result);
            break;
        case BO_mul:
            result = FLOAT_VAL(BINOP_LEFT(node)) * FLOAT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTfloat(result);
            break;
        case BO_div:
            result = FLOAT_VAL(BINOP_LEFT(node)) / FLOAT_VAL(BINOP_RIGHT(node));
            free(node);
            return ASTfloat(result);
            break;
        }
    }
    return node;
}

node_st *OPT_CFprogram(node_st *node)
{
    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
