#include "ccngen/ast.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <stdlib.h>

node_st *OPT_ASbinop(node_st *node)
{
    BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
    BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));

    if (BINOP_OP(node) == BO_mul)
    {
        if (NODE_TYPE(BINOP_LEFT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_LEFT(node)) == 1)
            {
                node_st *right = BINOP_RIGHT(node);
                BINOP_RIGHT(node) = NULL;
                CCNfree(node);
                return right;
            }
            else if (INT_VAL(BINOP_LEFT(node)) == 0)
            {
                CCNfree(node);
                return ASTint(0);
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_RIGHT(node)) == 1)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                return left;
            }
            else if (INT_VAL(BINOP_RIGHT(node)) == 0)
            {
                CCNfree(node);
                return ASTint(0);
            }
        }

        if (NODE_TYPE(BINOP_LEFT(node)) == NT_FLOAT)
        {
            if (FLOAT_VAL(BINOP_LEFT(node)) == 1.0)
            {
                node_st *right = BINOP_RIGHT(node);
                BINOP_RIGHT(node) = NULL;
                CCNfree(node);
                return right;
            }
            else if (FLOAT_VAL(BINOP_LEFT(node)) == 0.0)
            {
                CCNfree(node);
                return ASTfloat(0.0);
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
        {
            if (FLOAT_VAL(BINOP_RIGHT(node)) == 1.0)
            {
                node_st *right = BINOP_RIGHT(node);
                BINOP_RIGHT(node) = NULL;
                CCNfree(node);
                return right;
            }
            else if (FLOAT_VAL(BINOP_RIGHT(node)) == 0.0)
            {
                CCNfree(node);
                return ASTfloat(0.0);
            }
        }

        // TODO NT_BOOL
    }

    // TODO BO_add & BO_sub; NOTE neutral element 0 in both the same logic
    // TODO BO_div == 1 && BO_rem == 1; NOTE a/1 = a = a%1
    // TODO BO_and & BO_or

    return node;
}

node_st *OPT_ASprogram(node_st *node)
{
    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
