#include "ccngen/ast.h"
#include "release_assert.h"
#include "to_string.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccn/phase_driver.h>
#include <ccngen/enum.h>
#include <limits.h>
#include <stdbool.h>

static void warn_float_result_infnan(node_st *node, double val, double left, double right, char op)
{
    struct ctinfo info = NODE_TO_CTINFO(node);
    CTIobj(CTI_WARN, true, info, "Encountered '%f' as optimization result of '%f %c %f'.", val,
           left, op, right);
}

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static void update_location_info(node_st *target, node_st *left, node_st *right)
{
    release_assert(target != NULL);
    release_assert(left != NULL);
    release_assert(right != NULL);
    NODE_BLINE(target) = MIN(NODE_BLINE(left), NODE_BLINE(right));
    NODE_BCOL(target) = MIN(NODE_BCOL(left), NODE_BCOL(right));
    NODE_ELINE(target) = MIN(NODE_ELINE(left), NODE_ELINE(right));
    NODE_ECOL(target) = MIN(NODE_ECOL(left), NODE_ECOL(right));
    if (NODE_FILENAME(target) == NULL)
    {
        NODE_FILENAME(target) = STRcpy(NODE_FILENAME(left));
    }
}

node_st *OPT_CFbinop(node_st *node)
{
    BINOP_LEFT(node) = TRAVopt(BINOP_LEFT(node));
    BINOP_RIGHT(node) = TRAVopt(BINOP_RIGHT(node));

    if (NODE_TYPE(BINOP_LEFT(node)) == NT_INT && NODE_TYPE(BINOP_RIGHT(node)) == NT_INT)
    {

        int iresult;
        bool bresult;
        node_st *result = NULL;
        int left = INT_VAL(BINOP_LEFT(node));
        int right = INT_VAL(BINOP_RIGHT(node));
        struct ctinfo info = NODE_TO_CTINFO(node);

        char *str = node_to_string(node);
        free(str);

        switch (BINOP_OP(node))
        {
        case BO_NULL:
            release_assert(false);
            break;
        case BO_add:
            if (right > 0 && left > INT_MAX - right)
            {
                CTIobj(CTI_WARN, true, info,
                       "Encountered 'Addition Overflow' as optimization result of '%d + %d'. Using "
                       "INT_MAX=2147483647 for further optimization.",
                       left, right);
                iresult = INT_MAX;
            }
            else if (right < 0 && left < INT_MIN - right)
            {
                CTIobj(
                    CTI_WARN, true, info,
                    "Encountered 'Addition Underflow' as optimization result of '%d + %d'. Using "
                    "INT_MIN=-2147483648 for further optimization.",
                    left, right);
                iresult = INT_MIN;
            }
            else
            {
                iresult = left + right;
            }
            result = ASTint(iresult);
            break;
        case BO_sub:
            if (right < 0 && left > INT_MAX + right)
            {
                CTIobj(CTI_WARN, true, info,
                       "Encountered 'Substraction Overflow' as optimization result of '%d - %d'. "
                       "Using INT_MAX=2147483647 for further optimization.",
                       left, right);
                iresult = INT_MAX;
            }
            else if (right > 0 && left < INT_MIN + right)
            {
                CTIobj(CTI_WARN, true, info,
                       "Encountered 'Substraction Underflow' as optimization result of '%d - %d'. "
                       "Using "
                       "INT_MIN=-2147483648 for further optimization.",
                       left, right);
                iresult = INT_MIN;
            }
            else
            {
                iresult = left - right;
            }
            result = ASTint(iresult);
            break;
        case BO_mul:
            if ((right == -1 && left == INT_MIN) || (left == -1 && right == INT_MIN) ||
                (right != 0 && left > INT_MAX / right))
            {
                CTIobj(CTI_WARN, true, info,
                       "Encountered 'Multiplication Overflow' as optimization result of '%d * %d'. "
                       "Using INT_MAX=2147483647 for further optimization.",
                       left, right);
                iresult = INT_MAX;
            }
            else if (right != 0 && right != -1 && left < INT_MIN / right)
            {
                CTIobj(
                    CTI_WARN, true, info,
                    "Encountered 'Multiplication Underflow' as optimization result of '%d * %d'. "
                    "Using INT_MIN=-2147483648 for further optimization.",
                    left, right);
                iresult = INT_MIN;
            }
            else
            {
                iresult = left - right;
                iresult = left * right;
            }
            result = ASTint(iresult);
            break;
        case BO_div:
            if (right == 0)
            {
                CTIobj(CTI_WARN, true, info,
                       "Encountered 'Division by zero' as optimization result of '%d / %d'. Using "
                       "INT_MAX=2147483647 for further optimization.",
                       left, right);
                iresult = INT_MAX;
            }
            else if (right == -1 && left == INT_MIN)
            {
                CTIobj(CTI_WARN, true, info,
                       "Encountered 'Division Overflow' as optimization result of '%d / %d'. "
                       "Using INT_MAX=2147483647 for further optimization.",
                       left, right);
                iresult = INT_MAX;
            }
            else
            {
                iresult = left / right;
            }
            result = ASTint(iresult);
            break;
        case BO_mod:
            if (right == 0)
            {
                CTIobj(CTI_WARN, true, info,
                       "Encountered 'Modulo by zero' as optimization result of '%d %c %d'. Using "
                       "INT_MAX=2147483647 for further optimization.",
                       left, '%', right);
                iresult = INT_MAX;
            }
            else
            {
                iresult = left % right;
            }
            result = ASTint(iresult);
            break;
        case BO_lt:
            bresult = left < right;
            result = ASTbool(bresult);
            break;
        case BO_le:
            bresult = left <= right;
            result = ASTbool(bresult);
            break;
        case BO_gt:
            bresult = left > right;
            result = ASTbool(bresult);
            break;
        case BO_ge:
            bresult = left >= right;
            result = ASTbool(bresult);
            break;
        case BO_eq:
            bresult = left == right;
            result = ASTbool(bresult);
            break;
        case BO_ne:
            bresult = left != right;
            result = ASTbool(bresult);
            break;
        case BO_and:
        case BO_or:
            // Do not exist
            release_assert(false);
            break;
        }

        release_assert(result != NULL);
        update_location_info(result, BINOP_LEFT(node), BINOP_RIGHT(node));
        CCNcycleNotify();
        CCNfree(node);
        return result;
    }
    else if (NODE_TYPE(BINOP_LEFT(node)) == NT_FLOAT && NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT)
    {
        double fresult;
        bool bresult;
        node_st *result = NULL;
        double left = FLOAT_VAL(BINOP_LEFT(node));
        double right = FLOAT_VAL(BINOP_RIGHT(node));

        switch (BINOP_OP(node))
        {
        case BO_NULL:
            release_assert(false);
            break;
        case BO_add:
            fresult = left + right;
            warn_float_result_infnan(node, fresult, left, right, '+');
            result = ASTfloat(fresult);
            break;
        case BO_sub:
            fresult = left - right;
            warn_float_result_infnan(node, fresult, left, right, '-');
            result = ASTfloat(fresult);
            break;
        case BO_mul:
            fresult = left * right;
            warn_float_result_infnan(node, fresult, left, right, '*');
            result = ASTfloat(fresult);
            break;
        case BO_div:
            fresult = left / right;
            warn_float_result_infnan(node, fresult, left, right, '/');
            result = ASTfloat(fresult);
            break;
        case BO_lt:
            bresult = left < right;
            result = ASTbool(bresult);
            break;
        case BO_le:
            bresult = left <= right;
            result = ASTbool(bresult);
            break;
        case BO_gt:
            bresult = left > right;
            result = ASTbool(bresult);
            break;
        case BO_ge:
            bresult = left >= right;
            result = ASTbool(bresult);
            break;
        case BO_eq:
            bresult = left == right;
            result = ASTbool(bresult);
            break;
        case BO_ne:
            bresult = left != right;
            result = ASTbool(bresult);
            break;
        case BO_mod:
        case BO_and:
        case BO_or:
            // Do not exist.
            release_assert(false);
            break;
        }

        release_assert(result != NULL);
        update_location_info(result, BINOP_LEFT(node), BINOP_RIGHT(node));
        CCNcycleNotify();
        CCNfree(node);
        return result;
    }
    else if (NODE_TYPE(BINOP_LEFT(node)) == NT_BOOL && NODE_TYPE(BINOP_RIGHT(node)) == NT_BOOL)
    {
        node_st *result = NULL;
        bool bresult;

        switch (BINOP_OP(node))
        {
        case BO_NULL:
            release_assert(false);
            break;
        case BO_add:
            bresult = BOOL_VAL(BINOP_LEFT(node)) | BOOL_VAL(BINOP_RIGHT(node));
            result = ASTbool(bresult);
            break;
        case BO_mul:
            bresult = BOOL_VAL(BINOP_LEFT(node)) & BOOL_VAL(BINOP_RIGHT(node));
            result = ASTbool(bresult);
            break;
        case BO_eq:
            bresult = BOOL_VAL(BINOP_LEFT(node)) == BOOL_VAL(BINOP_RIGHT(node));
            result = ASTbool(bresult);
            break;
        case BO_ne:
            bresult = BOOL_VAL(BINOP_LEFT(node)) != BOOL_VAL(BINOP_RIGHT(node));
            result = ASTbool(bresult);
            break;
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

        release_assert(result != NULL);
        update_location_info(result, BINOP_LEFT(node), BINOP_RIGHT(node));
        CCNcycleNotify();
        CCNfree(node);
        return result;
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
            CCNcycleNotify();
            return child;
        }
        else
        {
            node_st *child = TERNARY_PFALSE(node);
            TERNARY_PFALSE(node) = NULL;
            CCNfree(node);
            CCNcycleNotify();
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
