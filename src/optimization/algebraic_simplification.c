#include "ccngen/ast.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <stdio.h>

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
                node = BINOP_RIGHT(node);
                return node;
            }
            else if (INT_VAL(BINOP_LEFT(node)) == 0)
            {
                node = ASTint(0);
                return node;
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_RIGHT(node)) == 1)
            {
                node = BINOP_LEFT(node);
                return node;
            }
            else if (INT_VAL(BINOP_RIGHT(node)) == 0)
            {
                node = ASTint(0);
                return node;
            }
        }

        if (NODE_TYPE(BINOP_LEFT(node)) == NT_FLOAT)
        {
            if (FLOAT_VAL(BINOP_LEFT(node)) == 1)
            {
                node = BINOP_RIGHT(node);
                return node;
            }
            else if (FLOAT_VAL(BINOP_LEFT(node)) == 0)
            {
                node = ASTfloat(0.0);
                return node;
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
        {
            if (FLOAT_VAL(BINOP_RIGHT(node)) == 1)
            {
                node = BINOP_LEFT(node);
                return node;
            }
            else if (FLOAT_VAL(BINOP_RIGHT(node)) == 0)
            {
                node = ASTfloat(0.0);
                return node;
            }
        }
    }

    return node;
}

node_st *OPT_ASprogram(node_st *node)
{
    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
