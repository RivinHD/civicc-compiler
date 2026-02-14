#include "ccngen/ast.h"
#include "palm/hash_table.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccn/phase_driver.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <string.h>

static htable_stptr current = NULL;
static htable_stptr sideeffect_table = NULL;
static node_st *sideeffected_expr = NULL;
static bool check_sideeffects = false;
static enum DataType sideeffect_type = DT_NULL;

static void reset()
{
    current = NULL;
    sideeffect_table = NULL;
    sideeffected_expr = NULL;
    check_sideeffects = false;
    sideeffect_type = DT_NULL;
}

node_st *OPT_ASproccall(node_st *node)
{
    TRAVchildren(node);
    if (!check_sideeffects)
    {
        return node;
    }

    node_st *fun = deep_lookup(current, VAR_NAME(PROCCALL_VAR(node)));
    release_assert(fun != NULL);
    enum sideeffect effect = check_fun_sideeffect(fun, sideeffect_table);
    release_assert(effect != SEFF_NULL);
    release_assert(effect != SEFF_PROCESSING);

    if (effect == SEFF_NO)
    {
        // Can remove no sideeffects
        return node;
    }

    node_st *funheader =
        NODE_TYPE(fun) == NT_FUNDEF ? FUNDEF_FUNHEADER(fun) : FUNDEC_FUNHEADER(fun);
    enum DataType type = FUNHEADER_TYPE(funheader);

    // The actual binop does not matter, either add or mul is valid as it is defined on bool,
    // float, int.
    // We need to ensure the left associativity to execute the proccalls in the correct order.
    node_st *expr = node;
    if (type != sideeffect_type)
    {
        switch (sideeffect_type)
        {
        // The actual replace value does not matter it only needs the be the correct type.
        case DT_NULL:
        case DT_void:
            release_assert(false);
            break;
        case DT_bool:
            sideeffected_expr = sideeffected_expr == NULL
                                    ? ASTpop(node, ASTbool(false), type, true)
                                    : ASTpop(node, sideeffected_expr, type, true);
            break;
        case DT_int:
            sideeffected_expr = sideeffected_expr == NULL
                                    ? ASTpop(node, ASTint(0), type, true)
                                    : ASTpop(node, sideeffected_expr, type, true);
            break;
        case DT_float:
            sideeffected_expr = sideeffected_expr == NULL
                                    ? ASTpop(node, ASTfloat(0.0), type, true)
                                    : ASTpop(node, sideeffected_expr, type, true);
            break;
        }
    }
    else
    {
        sideeffected_expr = sideeffected_expr == NULL
                                ? expr
                                : ASTbinop(sideeffected_expr, expr, BO_add, sideeffect_type);
    }

    CCNcycleNotify();
    return NULL;
}

node_st *OPT_ASternary(node_st *node)
{
    if (!check_sideeffects)
    {
        TRAVchildren(node);
        return node;
    }

    // We are not allowed to optimize away the ternary expression if it contains sideffecting
    // expressions in its ptrue or pfalse childs, as it conditions the execution of these.
    node_st *parent_sideeffect_expr = sideeffected_expr;

    sideeffected_expr = NULL;
    TERNARY_PFALSE(node) = TRAVopt(TERNARY_PFALSE(node));
    node_st *pfalse_sideeffected_expr = sideeffected_expr;

    sideeffected_expr = NULL;
    TERNARY_PTRUE(node) = TRAVopt(TERNARY_PTRUE(node));
    node_st *ptrue_sideeffected_expr = sideeffected_expr;

    if (pfalse_sideeffected_expr != NULL || ptrue_sideeffected_expr != NULL)
    {
        CCNfree(TERNARY_PFALSE(node));
        CCNfree(TERNARY_PTRUE(node));

        TERNARY_PFALSE(node) = pfalse_sideeffected_expr;
        TERNARY_PTRUE(node) = ptrue_sideeffected_expr;

        // We need to add the terneray to the sideeffects.
        node_st *fill_expr = NULL;
        if (TERNARY_PTRUE(node) == NULL || TERNARY_PFALSE(node) == NULL)
        {
            // The actual replace value does not matter it only needs the be the correct type.
            switch (sideeffect_type)
            {
            case DT_NULL:
            case DT_void:
                release_assert(false);
                break;
            case DT_bool:
                fill_expr = ASTbool(false);
                break;
            case DT_int:
                fill_expr = ASTint(0);
                break;
            case DT_float:
                fill_expr = ASTfloat(0.0);
                break;
            }
        }

        if (TERNARY_PFALSE(node) == NULL)
        {
            release_assert(TERNARY_PTRUE(node) != NULL);
            release_assert(fill_expr != NULL);
            TERNARY_PFALSE(node) = fill_expr;
        }
        else if (TERNARY_PTRUE(node) == NULL)
        {
            release_assert(fill_expr != NULL);
            TERNARY_PTRUE(node) = fill_expr;
        }

        parent_sideeffect_expr =
            parent_sideeffect_expr == NULL
                ? node
                : ASTbinop(parent_sideeffect_expr, node, BO_add, sideeffect_type);

        sideeffected_expr = parent_sideeffect_expr;
        CCNcycleNotify();
        return NULL;
    }
    else
    {
        sideeffected_expr = parent_sideeffect_expr;
        // Extract the sideffecting from the prediction, because we don't care about the ptrue or
        // pfalse values, we can completely remove the terneray operation.
        TERNARY_PRED(node) = TRAVopt(TERNARY_PRED(node));
        return node;
    }
}

node_st *OPT_AScast(node_st *node)
{
    if (!check_sideeffects)
    {
        TRAVchildren(node);
        return node;
    }

    enum DataType parent_sideeffect_type = sideeffect_type;
    node_st *parent_sideeffect_expr = sideeffected_expr;
    release_assert(CAST_TYPE(node) == sideeffect_type);
    sideeffect_type = CAST_FROMTYPE(node);
    sideeffected_expr = NULL;
    TRAVchildren(node);

    if (sideeffected_expr == NULL)
    {
        // Can remove no sideeffects
        sideeffect_type = parent_sideeffect_type;
        sideeffected_expr = parent_sideeffect_expr;
        return node;
    }

    if (CAST_FROMTYPE(node) != parent_sideeffect_type)
    {
        node_st *cast_expr = ASTcast(sideeffected_expr, CAST_TYPE(node), CAST_FROMTYPE(node));
        if ((CAST_FROMTYPE(node) == DT_int && CAST_TYPE(node) == DT_float) ||
            (CAST_FROMTYPE(node) == DT_float && CAST_TYPE(node) == DT_int))
        {
            parent_sideeffect_expr =
                parent_sideeffect_expr == NULL
                    ? cast_expr
                    : ASTbinop(parent_sideeffect_expr, cast_expr, BO_add, sideeffect_type);
        }
        else
        {
            release_assert(false);
        }
    }
    else
    {
        // We can ignore the cast expression and directly append to the sideeffect epxressions
        parent_sideeffect_expr =
            parent_sideeffect_expr == NULL
                ? sideeffected_expr
                : ASTbinop(parent_sideeffect_expr, sideeffected_expr, BO_add, sideeffect_type);
    }

    sideeffect_type = parent_sideeffect_type;
    sideeffected_expr = parent_sideeffect_expr;
    CCNcycleNotify();
    return node;
}

node_st *OPT_ASbinop(node_st *node)
{

    BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
    BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));

    if (check_sideeffects)
    {
        return node;
    }

    // TODO add check when binop right type of left of right binop with zero we can simplify again
    // this can happen because we need to keep some binops for sideeffects. There the the zeroing
    // elements are placed at the right position for easier simplification as it does not matter to
    // the evaluation of binops. We do this in another optimization phase where we reordere the
    // binops. Note: We are only allowed to reorder float with multiplication of 0.0 otherwise we
    // might induce floating point precision errors.

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
                CCNcycleNotify();
                return right;
            }
            else if (INT_VAL(BINOP_LEFT(node)) == 0)
            {
                check_sideeffects = true;
                sideeffect_type = DT_int;
                BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));
                check_sideeffects = false;
                sideeffect_type = DT_NULL;
                node_st *expr = ASTint(0);
                if (sideeffected_expr != NULL)
                {
                    expr = ASTbinop(sideeffected_expr, expr, BO_mul, DT_int);
                    sideeffected_expr = NULL;
                }
                CCNfree(node);
                CCNcycleNotify();
                return expr;
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_RIGHT(node)) == 1)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                CCNcycleNotify();
                return left;
            }
            else if (INT_VAL(BINOP_RIGHT(node)) == 0)
            {
                check_sideeffects = true;
                sideeffect_type = DT_int;
                BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
                check_sideeffects = false;
                sideeffect_type = DT_NULL;
                node_st *expr = ASTint(0);
                if (sideeffected_expr != NULL)
                {
                    expr = ASTbinop(sideeffected_expr, expr, BO_mul, DT_int);
                    sideeffected_expr = NULL;
                }
                CCNfree(node);
                CCNcycleNotify();
                return expr;
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
                CCNcycleNotify();
                return right;
            }
            else if (FLOAT_VAL(BINOP_LEFT(node)) == 0.0)
            {
                check_sideeffects = true;
                sideeffect_type = DT_float;
                BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));
                check_sideeffects = false;
                sideeffect_type = DT_NULL;
                node_st *expr = ASTfloat(0.0);
                if (sideeffected_expr != NULL)
                {
                    expr = ASTbinop(sideeffected_expr, expr, BO_mul, DT_float);
                    sideeffected_expr = NULL;
                }
                CCNfree(node);
                CCNcycleNotify();
                return expr;
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
        {
            if (FLOAT_VAL(BINOP_RIGHT(node)) == 1.0)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                CCNcycleNotify();
                return left;
            }
            else if (FLOAT_VAL(BINOP_RIGHT(node)) == 0.0)
            {
                check_sideeffects = true;
                sideeffect_type = DT_float;
                BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
                check_sideeffects = false;
                sideeffect_type = DT_NULL;
                node_st *expr = ASTfloat(0.0);
                if (sideeffected_expr != NULL)
                {
                    expr = ASTbinop(sideeffected_expr, expr, BO_mul, DT_float);
                    sideeffected_expr = NULL;
                }
                CCNfree(node);
                CCNcycleNotify();
                return expr;
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
                CCNcycleNotify();
                return right;
            }
            else
            {
                check_sideeffects = true;
                sideeffect_type = DT_bool;
                BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));
                check_sideeffects = false;
                sideeffect_type = DT_NULL;
                node_st *expr = ASTbool(false);
                if (sideeffected_expr != NULL)
                {
                    expr = ASTbinop(sideeffected_expr, expr, BO_mul, DT_bool);
                    sideeffected_expr = NULL;
                }
                CCNfree(node);
                CCNcycleNotify();
                return expr;
            }
        }

        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_BOOL)
        {
            if (BOOL_VAL(BINOP_RIGHT(node)) == true)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                CCNcycleNotify();
                return left;
            }
            else
            {
                check_sideeffects = true;
                sideeffect_type = DT_bool;
                BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
                check_sideeffects = false;
                sideeffect_type = DT_NULL;
                node_st *expr = ASTbool(false);
                if (sideeffected_expr != NULL)
                {
                    expr = ASTbinop(sideeffected_expr, expr, BO_mul, DT_bool);
                    sideeffected_expr = NULL;
                }
                CCNfree(node);
                CCNcycleNotify();
                return expr;
            }
        }
    }
    else if (BINOP_OP(node) == BO_add || BINOP_OP(node) == BO_sub)
    {
        if (NODE_TYPE(BINOP_LEFT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_LEFT(node)) == 0)
            {
                node_st *right = BINOP_RIGHT(node);
                BINOP_RIGHT(node) = NULL;
                CCNfree(node);
                CCNcycleNotify();
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
                CCNcycleNotify();
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
                CCNcycleNotify();
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
                CCNcycleNotify();
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

                    check_sideeffects = true;
                    sideeffect_type = DT_bool;
                    BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));
                    check_sideeffects = false;
                    sideeffect_type = DT_NULL;
                    node_st *expr = left;
                    if (sideeffected_expr != NULL)
                    {
                        expr = ASTbinop(sideeffected_expr, expr, BO_add, DT_bool);
                        sideeffected_expr = NULL;
                    }
                    CCNfree(node);
                    CCNcycleNotify();
                    return expr;
                }
            }

            if (NODE_TYPE(BINOP_RIGHT(node)) == NT_BOOL)
            {
                if (BOOL_VAL(BINOP_RIGHT(node)) == true)
                {
                    node_st *right = BINOP_RIGHT(node);
                    BINOP_RIGHT(node) = NULL;

                    check_sideeffects = true;
                    sideeffect_type = DT_bool;
                    BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
                    check_sideeffects = false;
                    sideeffect_type = DT_NULL;
                    node_st *expr = right;
                    if (sideeffected_expr != NULL)
                    {
                        expr = ASTbinop(sideeffected_expr, expr, BO_add, DT_bool);
                        sideeffected_expr = NULL;
                    }
                    CCNfree(node);
                    CCNcycleNotify();
                    return expr;
                }
            }
        }
    }
    else if (BINOP_OP(node) == BO_div || BINOP_OP(node) == BO_mod)
    {
        if (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
        {
            if (INT_VAL(BINOP_RIGHT(node)) == 1)
            {
                node_st *left = BINOP_LEFT(node);
                BINOP_LEFT(node) = NULL;
                CCNfree(node);
                CCNcycleNotify();
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
                CCNcycleNotify();
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
                    CCNcycleNotify();
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
                    CCNcycleNotify();
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
    if (check_sideeffects)
    {
        return node;
    }

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
            CCNcycleNotify();
            return child;
        }
        else if (child_type == NT_FLOAT)
        {
            MONOP_LEFT(node) = NULL;
            CCNfree(node);
            FLOAT_VAL(child) = -FLOAT_VAL(child);
            CCNcycleNotify();
            return child;
        }
        else if (child_type == NT_MONOP)
        {
            release_assert(MONOP_OP(child) == MO_neg);
            node_st *child_child = MONOP_LEFT(child);
            MONOP_LEFT(child) = NULL;
            CCNfree(node);
            CCNcycleNotify();
            return child_child;
        }
        break;
    case MO_not:
        if (child_type == NT_BOOL)
        {
            MONOP_LEFT(node) = NULL;
            CCNfree(node);
            BOOL_VAL(child) = !BOOL_VAL(child);
            CCNcycleNotify();
            return child;
        }
        else if (child_type == NT_MONOP)
        {
            release_assert(MONOP_OP(child) == MO_not);
            node_st *child_child = MONOP_LEFT(child);
            MONOP_LEFT(child) = NULL;
            CCNfree(node);
            CCNcycleNotify();
            return child_child;
        }
        break;
    }

    return node;
}

node_st *OPT_ASfundef(node_st *node)
{
    if (check_sideeffects)
    {
        TRAVchildren(node);
        return node;
    }

    htable_stptr parent_current = current;
    current = FUNDEF_SYMBOLS(node);
    TRAVchildren(node);
    current = parent_current;
    return node;
}

node_st *OPT_ASprogram(node_st *node)
{
    reset();
    sideeffect_table = HTnew_Ptr(2 << 8);
    current = PROGRAM_SYMBOLS(node);
    TRAVopt(PROGRAM_DECLS(node));
    HTdelete(sideeffect_table);
    return node;
}
