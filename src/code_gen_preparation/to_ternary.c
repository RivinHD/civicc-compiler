#include "ccngen/ast.h"
#include "release_assert.h"
#include "stdio.h"

#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>

node_st *CGP_TTbinop(node_st *node)
{
    TRAVchildren(node);
    node_st *new_node = NULL;

    switch (BINOP_OP(node))
    {
    case BO_NULL:
        release_assert(false);
        return node;
    case BO_add:
    case BO_sub:
    case BO_mul:
    case BO_div:
    case BO_mod:
    case BO_lt:
    case BO_le:
    case BO_gt:
    case BO_ge:
    case BO_eq:
    case BO_ne:
        // Nothing to do
        return node;
    case BO_and:
        new_node = ASTternary(BINOP_LEFT(node), BINOP_RIGHT(node), ASTbool(false));
        break;
    case BO_or:
        new_node = ASTternary(BINOP_LEFT(node), ASTbool(true), BINOP_RIGHT(node));
        break;
    }

    release_assert(new_node != NULL);

    BINOP_LEFT(node) = NULL;
    BINOP_RIGHT(node) = NULL;
    CCNfree(node);

    return new_node;
}

node_st *CGP_TTcast(node_st *node)
{
    TRAVchildren(node);

    node_st *new_node = NULL;
    switch (CAST_TYPE(node))
    {

    case DT_NULL:
    case DT_void:
        release_assert(false);
        break;
    case DT_bool:
        switch (CAST_FROMTYPE(node))
        {
        case DT_NULL:
        case DT_void:
            release_assert(false);
            break;
        case DT_bool:
            // Remove cast, because cast does nothing
            new_node = CAST_EXPR(node);
            break;
        case DT_int:
            new_node = ASTternary(ASTbinop(CAST_EXPR(node), ASTint(0), BO_ne), ASTbool(true),
                                  ASTbool(false));
            break;
        case DT_float:
            new_node = ASTternary(ASTbinop(CAST_EXPR(node), ASTfloat(0.0), BO_ne), ASTbool(true),
                                  ASTbool(false));
            break;
        }
        break;
    case DT_int:
        switch (CAST_FROMTYPE(node))
        {
        case DT_NULL:
        case DT_void:
            release_assert(false);
            break;
        case DT_bool:
            new_node = ASTternary(CAST_EXPR(node), ASTint(1), ASTint(0));
            break;
        case DT_int:
            // Remove cast, because cast does nothing
            new_node = CAST_EXPR(node);
            break;
        case DT_float:
            return node; // use vm-builtin conversion
        }
        break;
    case DT_float:
        switch (CAST_FROMTYPE(node))
        {
        case DT_NULL:
        case DT_void:
            release_assert(false);
        case DT_bool:
            new_node = ASTternary(CAST_EXPR(node), ASTfloat(1.0), ASTfloat(0.0));
            break;
        case DT_int:
            return node; // use vm-builtin conversion
        case DT_float:
            // Remove cast, because cast does nothing
            new_node = CAST_EXPR(node);
            break;
        }
        break;
    }

    release_assert(new_node != NULL);

    CAST_EXPR(node) = NULL;
    CCNfree(node);

    return new_node;
}
