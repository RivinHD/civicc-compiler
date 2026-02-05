#include "ccngen/ast.h"
#include "release_assert.h"
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
        // NT_INT
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

        // NT_FLOAT
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
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                return left;
            }
            else if (FLOAT_VAL(BINOP_RIGHT(node)) == 0.0)
            {
                CCNfree(node);
                return ASTfloat(0.0);
            }
        }

        // NT_BOOL
        if (NODE_TYPE(BINOP_LEFT(node)) == NT_BOOL)
        {
            if (BOOL_VAL(BINOP_LEFT(node)) == true)
            {
                node_st *right = BINOP_RIGHT(node);
                BINOP_RIGHT(node) = NULL;
                CCNfree(node);
                return right;
            }
            else
            {
                CCNfree(node);
                return ASTbool(false);
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_BOOL)
        {
            if (BOOL_VAL(BINOP_RIGHT(node)) == true)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                return left;
            }
            else
            {
                CCNfree(node);
                return ASTbool(false);
            }
        }
    }
    else if (BINOP_OP(node) == BO_add || BINOP_OP(node) == BO_sub)
    {
        // TODO BO_add & BO_sub; NOTE neutral element 0 in both the same logic
        if (NODE_TYPE(BINOP_LEFT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_LEFT(node)) == 0)
            {
                node_st *right = BINOP_RIGHT(node);
                BINOP_RIGHT(node) = NULL;
                CCNfree(node);
                return right;
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_RIGHT(node)) == 0)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                return left;
            }
        }

        // NT_FLOAT
        if (NODE_TYPE(BINOP_LEFT(node)) == NT_FLOAT)
        {
            if (FLOAT_VAL(BINOP_LEFT(node)) == 0.0)
            {
                node_st *right = BINOP_RIGHT(node);
                BINOP_RIGHT(node) = NULL;
                CCNfree(node);
                return right;
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
        {
            if (FLOAT_VAL(BINOP_RIGHT(node)) == 0.0)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                return left;
            }
        }

        // NT_BOOL
        if (BINOP_OP(node) == BO_add)
        {
            if (NODE_TYPE(BINOP_LEFT(node)) == NT_BOOL)
            {
                if (BOOL_VAL(BINOP_LEFT(node)) == true)
                {
                    node_st *left = BINOP_LEFT(node);
                    BINOP_LEFT(node) = NULL;
                    CCNfree(node);
                    return left;
                }
            }

            if (NODE_TYPE(BINOP_RIGHT(node)) == NT_BOOL)
            {
                if (BOOL_VAL(BINOP_RIGHT(node)) == true)
                {
                    node_st *right = BINOP_RIGHT(node);
                    BINOP_RIGHT(node) = NULL;
                    CCNfree(node);
                    return right;
                }
            }
        }
    }
    else if (BINOP_OP(node) == BO_div || BINOP_OP(node) == BO_mod)
    {
        // TODO BO_div == 1 && BO_rem == 1; NOTE a/1 = a = a%1 (auch -1)
        // DIV
        // NT_INT
        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_RIGHT(node)) == 1)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                return left;
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_RIGHT(node)) == -1)
            {
                node_st *left = BINOP_LEFT(node);
                node_st *new_left = ASTmonop(left, MO_neg);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                return new_left;
            }
        }

        // NT_FLOAT
        if (BINOP_OP(node) == BO_div)
        {

            if (NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
            {
                if (FLOAT_VAL(BINOP_RIGHT(node)) == 1.0)
                {
                    node_st *left = BINOP_LEFT(node);
                    BINOP_LEFT(node) = NULL;
                    CCNfree(node);
                    return left;
                }
            }

            if (NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
            {
                if (FLOAT_VAL(BINOP_RIGHT(node)) == -1.0)
                {
                    node_st *left = BINOP_LEFT(node);
                    node_st *new_left = ASTmonop(left, MO_neg);
                    BINOP_LEFT(node) = NULL;
                    CCNfree(node);
                    return new_left;
                }
            }
        }
    }
    else if (BINOP_OP(node) == BO_and || BINOP_OP(node) == BO_or)
    {
        release_assert(false);
    }

    return node;
}

node_st *OPT_ASmonop(node_st *node)
{
    TRAVchildren(node);
    node_st *child = MONOP_LEFT(node);
    enum ccn_nodetype child_type = NODE_TYPE(child);

    switch (MONOP_OP(node))
    {
    case MO_NULL:
        release_assert(false);
        break;
    case MO_neg:
        if (child_type == NT_INT)
        {
            MONOP_LEFT(node) = NULL;
            CCNfree(node);
            INT_VAL(child) = -INT_VAL(child);
            return child;
        }
        else if (child_type == NT_FLOAT)
        {
            MONOP_LEFT(node) = NULL;
            CCNfree(node);
            FLOAT_VAL(child) = -FLOAT_VAL(child);
            return child;
        }
        else if (child_type == NT_MONOP)
        {
            release_assert(MONOP_OP(child) == MO_neg);
            node_st *child_child = MONOP_LEFT(child);
            MONOP_LEFT(child) = NULL;
            CCNfree(node);
            return child_child;
        }
        break;
    case MO_not:
        if (child_type == NT_BOOL)
        {
            MONOP_LEFT(node) = NULL;
            CCNfree(node);
            BOOL_VAL(child) = !BOOL_VAL(child);
            return child;
        }
        else if (child_type == NT_MONOP)
        {
            release_assert(MONOP_OP(child) == MO_not);
            node_st *child_child = MONOP_LEFT(child);
            MONOP_LEFT(child) = NULL;
            CCNfree(node);
            return child_child;
        }
        break;
    }

    return node;
}

node_st *OPT_ASprogram(node_st *node)
{
    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
