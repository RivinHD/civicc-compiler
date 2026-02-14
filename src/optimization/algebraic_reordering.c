#include "definitions.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccn/phase_driver.h>
#include <ccngen/ast.h>
#include <ccngen/enum.h>

node_st *OPT_ARbinop(node_st *node)
{
    TRAVchildren(node);
    enum BinOpType op = BINOP_OP(node);
    node_st *left = BINOP_LEFT(node);
    node_st *right = BINOP_RIGHT(node);
    enum DataType type = BINOP_ARGTYPE(node);

    enum ccn_nodetype ntype = NT_NULL;
    switch (type)
    {
    case DT_NULL:
    case DT_void:
        release_assert(false);
        break;
    case DT_bool:
        ntype = NT_BOOL;
        break;
    case DT_int:
        ntype = NT_INT;
        break;
    case DT_float:
        ntype = NT_FLOAT;
        break;
    }
    release_assert(ntype != NT_NULL);

    switch (op)
    {
    case BO_NULL:
    case BO_and:
    case BO_or:
        release_assert(false);
        break;
    case BO_add:
    case BO_sub:
    case BO_mul:
        // Reordering useful
        // expected example output: (expr + expr  + ... + const + const)
        // We have the following cases:
        // 1. Left & Right are binops
        // - check if they are the same kind
        // - check if their child have a constant that we can reorder
        // 2. Left is binop & Right is any expr
        // - check if they are the same kind
        // - check if left have a constant that we can reorder
        // 3. Left is any expr & Right is binop
        // - check if they are the same kind
        // - check if right have a constant that we can reorder
        // 4. Left is constant & right is any expr
        // - swap left with right
        // 5. Rigth is any expr & left is binop
        // - do nothing already correct
        // 6. Left is any expr & right is any expr
        // - do nothing, we cannot optimize
        // Note: We are not allowed to reorder sub and add, because we could cause an unexpext
        // overflow
        if (NODE_TYPE(left) == NT_BINOP && NODE_TYPE(right) == NT_BINOP && BINOP_OP(left) == op &&
            BINOP_OP(right) == op)
        {
            // We need type int or bool, or float but op == MUL and one constant is 0.0
            // ((expr + expr) + (expr + expr)) -> (((expr + expr) + expr) + expr)
            if (type == DT_int || type == DT_bool)
            {
                node_st *rleft = BINOP_LEFT(right);
                BINOP_RIGHT(node) = rleft;
                BINOP_LEFT(right) = node;
                node = right;
                CCNcycleNotify();
            }
            else if (op == BO_mul)
            {
                node_st *zero = NULL;
                release_assert(type == DT_float);
                if (NODE_TYPE(BINOP_LEFT(left)) == NT_FLOAT && FLOAT_VAL(BINOP_LEFT(left)) == 0.0)
                {
                    // ((0.0 + expr) + (expr + expr)) -> ((expr + expr) + (expr + 0.0))
                    zero = BINOP_LEFT(left);
                    BINOP_LEFT(left) = BINOP_RIGHT(left);
                    BINOP_RIGHT(left) = BINOP_LEFT(right);
                    BINOP_LEFT(right) = BINOP_RIGHT(right);
                    BINOP_RIGHT(right) = zero;
                }
                else if (NODE_TYPE(BINOP_RIGHT(left)) == NT_FLOAT &&
                         FLOAT_VAL(BINOP_RIGHT(left)) == 0.0)
                {
                    // ((expr + 0.0) + (expr + expr)) -> ((expr + expr) + (expr + 0.0))
                    zero = BINOP_RIGHT(left);
                    BINOP_RIGHT(left) = BINOP_LEFT(right);
                    BINOP_LEFT(right) = BINOP_RIGHT(right);
                    BINOP_RIGHT(right) = zero;
                }
                else if (NODE_TYPE(BINOP_LEFT(right)) == NT_FLOAT &&
                         FLOAT_VAL(BINOP_LEFT(right)) == 0.0)
                {
                    // ((expr + expr) + (0.0 + expr)) -> ((expr + expr) + (expr + 0.0))
                    zero = BINOP_LEFT(right);
                    BINOP_LEFT(right) = BINOP_RIGHT(right);
                    BINOP_RIGHT(right) = zero;
                }
                else if (NODE_TYPE(BINOP_RIGHT(right)) == NT_FLOAT &&
                         FLOAT_VAL(BINOP_RIGHT(right)) == 0.0)
                {
                    // ((expr + expr) + (expr + 0.0)) :)
                    zero = BINOP_LEFT(left);
                }

                if (zero != NULL)
                {
                    // ((expr + expr) + (expr + 0.0)) -> (((expr + expr) + expr) + 0.0)
                    node_st *rleft = BINOP_LEFT(right);
                    BINOP_RIGHT(node) = rleft;
                    BINOP_LEFT(right) = node;
                    node = right;
                    CCNcycleNotify();
                }
            }
        }
        else if (NODE_TYPE(right) == NT_BINOP && NODE_TYPE(left) != NT_BINOP)
        {
            // (expr + (expr + expr)) -> ((expr + expr) + expr)
            if (type == DT_bool || type == DT_int)
            {
                BINOP_LEFT(node) = right;
                BINOP_RIGHT(node) = BINOP_RIGHT(right);
                BINOP_RIGHT(right) = BINOP_LEFT(right);
                BINOP_LEFT(right) = left;
                CCNcycleNotify();
            }
            else if (op == BO_mul)
            {
                release_assert(type == DT_float);
                if (NODE_TYPE(left) == NT_FLOAT && FLOAT_VAL(left) == 0.0)
                {
                    // (0.0 + (expr + expr)) -> ((expr + expr) + 0.0)
                    node_st *zero = left;
                    BINOP_LEFT(node) = right;
                    BINOP_RIGHT(node) = zero;
                    CCNcycleNotify();
                }
                else if (NODE_TYPE(BINOP_LEFT(right)) == NT_FLOAT &&
                         FLOAT_VAL(BINOP_LEFT(right)) == 0.0)
                {
                    // (expr + (0.0 + expr)) -> ((expr + expr) + 0.0)
                    node_st *zero = BINOP_LEFT(right);
                    BINOP_LEFT(node) = right;
                    BINOP_RIGHT(node) = zero;
                    BINOP_LEFT(right) = left;
                    CCNcycleNotify();
                }
                else if (NODE_TYPE(BINOP_RIGHT(right)) == NT_FLOAT &&
                         FLOAT_VAL(BINOP_RIGHT(right)) == 0.0)
                {
                    // (expr + (expr + 0.0)) -> ((expr + expr) + 0.0)
                    node_st *zero = BINOP_RIGHT(right);
                    BINOP_LEFT(node) = right;
                    BINOP_RIGHT(node) = zero;
                    BINOP_RIGHT(right) = BINOP_LEFT(right);
                    BINOP_LEFT(right) = left;
                    CCNcycleNotify();
                }
            }
        }

        // Update because of possible change through reordering
        TRAVchildren(node);
        left = BINOP_LEFT(node);
        right = BINOP_RIGHT(node);

        // Constants should already be at the right side of any child binop
        if (NODE_TYPE(left) == NT_BINOP && NODE_TYPE(right) != ntype &&
            NODE_TYPE(BINOP_RIGHT(left)) == ntype && (type == DT_bool || type == DT_int))
        {
            // ((expr + const) + expr) -> ((expr + expr) + const)
            node_st *lright = BINOP_RIGHT(left);
            BINOP_RIGHT(left) = BINOP_RIGHT(node);
            BINOP_RIGHT(node) = lright;
            CCNcycleNotify();
        }
        else if (NODE_TYPE(left) == NT_BINOP && type == DT_float && op == BO_mul &&
                 (NODE_TYPE(right) != ntype ||
                  (NODE_TYPE(right) == ntype && FLOAT_VAL(right) != 0.0)) &&
                 NODE_TYPE(BINOP_RIGHT(left)) == ntype && FLOAT_VAL(BINOP_RIGHT(left)) == 0.0)
        {
            // Reorder the zero to the right side
            // ((expr * 0.0) * expr/const) -> ((expr * expr/const) * 0.0)
            BINOP_RIGHT(node) = BINOP_RIGHT(left);
            BINOP_RIGHT(left) = right;
            CCNcycleNotify();
        }
        else if (NODE_TYPE(left) == ntype &&
                 (NODE_TYPE(right) != ntype ||
                  (type == DT_float && FLOAT_VAL(left) == 0.0 && FLOAT_VAL(right) != 0.0)) &&
                 op != BO_sub)
        {
            // (const + expr) -> (expr + const)
            BINOP_LEFT(node) = right;
            BINOP_RIGHT(node) = left;
            CCNcycleNotify();
        }
        break;
    case BO_div:
    case BO_mod:
    case BO_lt:
    case BO_le:
    case BO_gt:
    case BO_ge:
    case BO_eq:
    case BO_ne:
        break;
    }
    return node;
}
