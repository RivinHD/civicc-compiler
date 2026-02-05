#include "ccngen/ast.h"
#include "release_assert.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>

node_st *OPT_CFbinop(node_st *node)
{
    BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
    BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));

    if (NODE_TYPE(BINOP_LEFT(node)) == NT_INT && NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
    {
        int iresult;
        bool bresult;

        switch (BINOP_OP(node))
        {
        case BO_NULL:
            release_assert(false);
            break;
        case BO_add:
            iresult = INT_VAL(BINOP_LEFT(node)) + INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTint(iresult);
        case BO_sub:
            iresult = INT_VAL(BINOP_LEFT(node)) - INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTint(iresult);
        case BO_mul:
            iresult = INT_VAL(BINOP_LEFT(node)) * INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTint(iresult);
        case BO_div:
            iresult = INT_VAL(BINOP_LEFT(node)) / INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTint(iresult);
        case BO_mod:
            iresult = INT_VAL(BINOP_LEFT(node)) % INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTint(iresult);
        case BO_lt:
            bresult = INT_VAL(BINOP_LEFT(node)) < INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_le:
            bresult = INT_VAL(BINOP_LEFT(node)) <= INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_gt:
            bresult = INT_VAL(BINOP_LEFT(node)) > INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_ge:
            bresult = INT_VAL(BINOP_LEFT(node)) >= INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_eq:
            bresult = INT_VAL(BINOP_LEFT(node)) == INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_ne:
            bresult = INT_VAL(BINOP_LEFT(node)) != INT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_and:
        case BO_or:
            // Do not exist
            release_assert(false);
            break;
        }
    }
    else if (NODE_TYPE(BINOP_LEFT(node)) == NT_FLOAT && NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
    {
        double fresult;
        bool bresult;

        switch (BINOP_OP(node))
        {
        case BO_NULL:
            release_assert(false);
            break;
        case BO_add:
            fresult = FLOAT_VAL(BINOP_LEFT(node)) + FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTfloat(fresult);
        case BO_sub:
            fresult = FLOAT_VAL(BINOP_LEFT(node)) - FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTfloat(fresult);
        case BO_mul:
            fresult = FLOAT_VAL(BINOP_LEFT(node)) * FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTfloat(fresult);
        case BO_div:
            fresult = FLOAT_VAL(BINOP_LEFT(node)) / FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTfloat(fresult);
        case BO_lt:
            bresult = FLOAT_VAL(BINOP_LEFT(node)) < FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_le:
            bresult = FLOAT_VAL(BINOP_LEFT(node)) <= FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_gt:
            bresult = FLOAT_VAL(BINOP_LEFT(node)) > FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_ge:
            bresult = FLOAT_VAL(BINOP_LEFT(node)) >= FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_eq:
            bresult = FLOAT_VAL(BINOP_LEFT(node)) == FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_ne:
            bresult = FLOAT_VAL(BINOP_LEFT(node)) != FLOAT_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_mod:
        case BO_and:
        case BO_or:
            // Do not exist.
            release_assert(false);
            break;
        }
    }
    else if (NODE_TYPE(BINOP_LEFT(node)) == NT_BOOL && NODE_TYPE(BINOP_RIGHT(node)) == NT_BOOL)
    {
        bool bresult;
        switch (BINOP_OP(node))
        {
        case BO_NULL:
            release_assert(false);
            break;
        case BO_add:
            bresult = BOOL_VAL(BINOP_LEFT(node)) | BOOL_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_mul:
            bresult = BOOL_VAL(BINOP_LEFT(node)) & BOOL_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_eq:
            bresult = BOOL_VAL(BINOP_LEFT(node)) == BOOL_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_ne:
            bresult = BOOL_VAL(BINOP_LEFT(node)) != BOOL_VAL(BINOP_RIGHT(node));
            CCNfree(node);
            return ASTbool(bresult);
        case BO_and:
        case BO_or:
            // Should be a terneray
            release_assert(false);
            break;
        case BO_sub:
        case BO_div:
        case BO_mod:
        case BO_lt:
        case BO_le:
        case BO_gt:
        case BO_ge:
            // Do not exist
            release_assert(false);
            break;
        }
    }
    return node;
}

node_st *OPT_CFternary(node_st *node)
{
    TRAVchildren(node);

    node_st *pred = TERNARY_PRED(node);
    if (NODE_TYPE(pred) == NT_BOOL)
    {
        if (BOOL_VAL(pred) == true)
        {
            node_st *child = TERNARY_PTRUE(node);
            TERNARY_PTRUE(node) = NULL;
            CCNfree(node);
            return child;
        }
        else
        {
            node_st *child = TERNARY_PFALSE(node);
            TERNARY_PFALSE(node) = NULL;
            CCNfree(node);
            return child;
        }
    }

    return node;
}

node_st *OPT_CFprogram(node_st *node)
{
    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
