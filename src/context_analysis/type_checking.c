#include "ccngen/ast.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "to_string.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static htable_stptr current = NULL;
static bool anytype = false; // Allows an expression to set the type
static enum DataType type = DT_NULL;

static void type_check(node_st *node, const char *name, enum DataType expected, enum DataType value)
{
    release_assert(expected != DT_NULL);
    release_assert(value != DT_NULL);

    if (expected == value)
    {
        return;
    }

    struct ctinfo info = NODE_TO_CTINFO(node);
    info.filename = STRcpy(global.input_file);
    char *expected_str = datatype_to_string(expected);
    char *value_str = datatype_to_string(value);
    CTIobj(CTI_ERROR, true, info, "'%s' has type '%s' but expected type '%s'.", name, value_str,
           expected_str);
    free(info.filename);
    free(expected_str);
    free(value_str);
}

static bool check_nesting(unsigned int level, node_st *init)
{
    release_assert(level >= 1);
    release_assert(init != NULL);

    if (level == 0)
    {
        // Last level should be an expression
        if (NODE_TYPE(init) == NT_ARRAYINIT)
        {
            struct ctinfo info = NODE_TO_CTINFO(init);
            info.filename = STRcpy(global.input_file);
            CTIobj(CTI_ERROR, true, info, "Too many dimensions in array initalization.");
            free(info.filename);
            return false;
        }
        else
        {
            return true;
        }
    }

    if (NODE_TYPE(init) == NT_ARRAYINIT)
    {
        bool valid = true;
        // Each element should have 'level' nesting
        while (init != NULL && valid == true)
        {
            node_st *nesting = ARRAYINIT_EXPR(init);
            valid = check_nesting(level - 1, nesting);
            init = ARRAYINIT_NEXT(init);
        }

        return valid;
    }

    struct ctinfo info = NODE_TO_CTINFO(init);
    info.filename = STRcpy(global.input_file);
    CTIobj(CTI_ERROR, true, info,
           "Insufficient dimensions in array initalization, missing '%d' further dimensions.",
           level);
    free(info.filename);

    return false;
}

static unsigned int count_exprs(node_st *exprs)
{
    release_assert(NODE_TYPE(exprs) == NT_EXPRS);
    unsigned int exprs_count = 0;
    while (exprs != NULL)
    {
        exprs_count++;
        exprs = EXPRS_NEXT(exprs);
    }
    return exprs_count;
}

static bool check_dimensions(node_st *exprs, node_st *array_init)
{
    release_assert(NODE_TYPE(exprs) == NT_EXPRS);
    release_assert(NODE_TYPE(array_init) == NT_ARRAYINIT);

    unsigned int exprs_count = count_exprs(exprs);

    return check_nesting(exprs_count, array_init);
}

node_st *CA_TCbool(node_st *node)
{
    if (anytype)
    {
        type = DT_bool;
        anytype = false;
        return node;
    }
    else
    {
        char *str = BOOL_VAL(node) ? "false" : "true";
        type_check(node, str, type, DT_bool);
    }

    return node;
}

node_st *CA_TCfloat(node_st *node)
{
    if (anytype)
    {
        type = DT_float;
        anytype = false;
        return node;
    }
    else
    {
        char *str = STRfmt("%f", FLOAT_VAL(node));
        type_check(node, str, type, DT_float);
        free(str);
    }

    return node;
}

node_st *CA_TCint(node_st *node)
{
    if (anytype)
    {
        type = DT_int;
        anytype = false;
    }
    else
    {
        char *str = STRfmt("%d", INT_VAL(node));
        type_check(node, str, type, DT_int);
        free(str);
    }

    return node;
}

node_st *CA_TCvar(node_st *node)
{
    char *name = VAR_NAME(node);
    node_st *entry = deep_lookup(current, name);
    if (entry == NULL && CTIgetErrors() > 0)
    {
        // Missing entry due to error, skip check.
        return node;
    }
    release_assert(entry != NULL);
    enum DataType has_type = symbol_to_type(entry);

    if (anytype)
    {
        type = has_type;
        anytype = false;
    }
    else
    {
        type_check(node, name, type, has_type);
    }

    return node;
}

node_st *CA_TCcast(node_st *node)
{
    enum DataType has_type = CAST_TYPE(node);
    if (anytype)
    {
        type = has_type;
        anytype = false;
    }
    else
    {
        type_check(node, "cast expression", type, has_type);
    }

    enum DataType parent_type = type;
    type = DT_NULL;
    anytype = true;
    TRAVopt(CAST_EXPR(node));
    anytype = false;
    type = parent_type;
    return node;
}

node_st *CA_TCarrayexpr(node_st *node)
{
    TRAVopt(ARRAYEXPR_VAR(node));

    // Check all dimension need to be of type int
    enum DataType parent_type = type;
    type = DT_int;
    TRAVopt(ARRAYEXPR_DIMS(node));
    type = parent_type;
    return node;
}
node_st *CA_TCmonop(node_st *node)
{
    enum DataType parent_type = type;

    if (anytype)
    {
        // infer the type from the first argument
        TRAVopt(MONOP_LEFT(node));
        parent_type = type;
    }

    bool is_correct = true;
    switch (MONOP_OP(node))
    {
    case MO_not:
        is_correct = type == DT_bool;
        break;
    case MO_neg:
        is_correct = (type == DT_int) | (type == DT_float);
        break;
    case MO_NULL:
        release_assert(false);
        break;
    }

    if (is_correct)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        info.filename = STRcpy(global.input_file);
        char *op_str = monoptype_to_string(MONOP_OP(node));
        char *type_str = datatype_to_string(type);
        CTIobj(CTI_ERROR, true, info, "The operation '%s' is not defined on the type '%s'", op_str,
               type_str);
        free(info.filename);
        free(op_str);
        free(type_str);
    }

    TRAVopt(MONOP_LEFT(node));
    type = parent_type;
    return node;
}

node_st *CA_TCbinop(node_st *node)
{
    enum DataType parent_type = type;
    char *op_str = NULL;
    char *type_str = NULL;

    switch (BINOP_OP(node))
    {
    case BO_sub:
    case BO_div:
        if (anytype)
        {
            // infer the type from the first argument
            TRAVopt(BINOP_LEFT(node));
            parent_type = type;
        }

        if ((type != DT_int) | (type != DT_float))
        {
            struct ctinfo info = NODE_TO_CTINFO(node);
            info.filename = STRcpy(global.input_file);
            op_str = binoptype_to_string(BINOP_OP(node));
            type_str = datatype_to_string(type);
            CTIobj(CTI_ERROR, true, info, "The operation '%s' is not defined on the type '%s'",
                   op_str, type_str);
            free(info.filename);
        }
        break;
    case BO_add:
    case BO_mul:
        if (anytype)
        {
            // infer the type from the first argument
            TRAVopt(BINOP_LEFT(node));
            parent_type = type;
        }
        release_assert((type == DT_int) | (type == DT_float) | (type == DT_bool));
        break;
    case BO_mod:
        if (anytype)
        {
            // infer the type from the first argument
            TRAVopt(BINOP_LEFT(node));
            parent_type = type;
        }
        if (type != DT_int)
        {
            struct ctinfo info = NODE_TO_CTINFO(node);
            info.filename = STRcpy(global.input_file);
            op_str = binoptype_to_string(BINOP_OP(node));
            type_str = datatype_to_string(type);
            CTIobj(CTI_ERROR, true, info, "The operation '%s' is not defined on the type '%s'",
                   op_str, type_str);
            free(info.filename);
        }
        break;
    case BO_lt:
    case BO_le:
    case BO_gt:
    case BO_ge:
        // Always yield a boolean
        if (anytype)
        {
            parent_type = DT_bool;
        }
        op_str = binoptype_to_string(BINOP_OP(node));
        type_check(node, op_str, DT_bool, parent_type);

        // infer the type from the first argument
        anytype = true;
        TRAVopt(BINOP_LEFT(node));
        if ((type != DT_int) | (type != DT_float))
        {
            struct ctinfo info = NODE_TO_CTINFO(node);
            info.filename = STRcpy(global.input_file);
            type_str = datatype_to_string(type);
            CTIobj(CTI_ERROR, true, info, "The operation '%s' is not defined on the type '%s'",
                   op_str, type_str);
            free(info.filename);
        }
        break;
    case BO_eq:
    case BO_ne:
        // Always yield a boolean
        if (anytype)
        {
            parent_type = DT_bool;
        }
        op_str = binoptype_to_string(BINOP_OP(node));
        type_check(node, op_str, DT_bool, parent_type);
        anytype = true; // Check for constistency of both arguments
        break;
    case BO_and:
    case BO_or:
        // Always yield a boolean
        if (anytype)
        {
            parent_type = DT_bool;
        }
        op_str = binoptype_to_string(BINOP_OP(node));
        type_check(node, op_str, DT_bool, parent_type);
        type = DT_bool; // Both arguments should be a bool
        break;
    case BO_NULL:
        release_assert(false);
        break;
    }

    if (op_str != NULL)
    {
        free(op_str);
    }
    if (type_str != NULL)
    {
        free(type_str);
    }

    TRAVopt(BINOP_LEFT(node));
    TRAVopt(BINOP_RIGHT(node));
    type = parent_type;
    return node;
}

node_st *CA_TCproccall(node_st *node)
{
    node_st *var = PROCCALL_VAR(node);
    char *name = VAR_NAME(var);
    node_st *entry = deep_lookup(current, name);
    if (entry == NULL && CTIgetErrors() > 0)
    {
        // Missing entry due to error, skip check.
        return node;
    }
    release_assert(entry != NULL);
    enum DataType has_type = symbol_to_type(entry);
    if (anytype)
    {
        type = has_type;
        anytype = false;
    }
    else
    {
        type_check(var, name, type, has_type);
    }

    release_assert(NODE_TYPE(entry) == NT_FUNHEADER);
    unsigned int exprs_count = 0;
    unsigned int params_count = 0;
    node_st *exprs = PROCCALL_EXPRS(node);
    node_st *params = FUNHEADER_PARAMS(entry);
    if (exprs != NULL)
    {
        while (exprs != NULL && params != NULL)
        {
            enum DataType parent_type = type;
            type = PARAMS_TYPE(params);
            TRAVopt(EXPRS_EXPR(exprs));
            type = parent_type;

            params_count++;
            exprs_count++;
            params = PARAMS_NEXT(params);
            exprs = EXPRS_NEXT(exprs);
        }

        while (exprs != NULL)
        {
            exprs_count++;
            exprs = EXPRS_NEXT(exprs);
        }
    }

    while (params != NULL)
    {
        params_count++;
        params = PARAMS_NEXT(params);
    }

    if (params_count != exprs_count)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        info.filename = STRcpy(global.input_file);
        CTIobj(CTI_ERROR, true, info, "Expected '%d' arguments, but only got '%d'.", params_count,
               exprs_count);
        free(info.filename);
    }

    return node;
}

node_st *CA_TCforloop(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_int; // for loop should only contain integer
    TRAVopt(FORLOOP_COND(node));
    TRAVopt(FORLOOP_ITER(node));

    TRAVopt(FORLOOP_ASSIGN(node));
    type = parent_type;
    return node;
}

node_st *CA_TCassign(node_st *node)
{
    node_st *entry = deep_lookup(current, VAR_NAME(ASSIGN_VAR(node)));
    if (entry == NULL && CTIgetErrors() > 0)
    {
        // Missing entry due to error, skip check.
        return node;
    }
    release_assert(entry != NULL);
    enum DataType parent_type = type;
    type = symbol_to_type(entry);
    TRAVopt(ASSIGN_EXPR(node));
    type = parent_type;
    return node;
}

node_st *CA_TCarrayassign(node_st *node)
{
    char *name = VAR_NAME(ARRAYASSIGN_VAR(node));
    node_st *entry = deep_lookup(current, name);
    if (entry == NULL && CTIgetErrors() > 0)
    {
        // Missing entry due to error, skip check.
        return node;
    }
    release_assert(entry != NULL);
    release_assert(NODE_TYPE(entry) == NT_VARDEC);
    node_st *var = VARDEC_VAR(entry);

    release_assert(NODE_TYPE(var) == NT_ARRAYEXPR);
    unsigned int expected_count = count_exprs(ARRAYEXPR_DIMS(var));
    unsigned int actual_count = count_exprs(ARRAYASSIGN_VAR(node));

    if (expected_count != actual_count)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        info.filename = STRcpy(global.input_file);
        CTIobj(CTI_ERROR, true, info,
               "Array '%s' has '%d' dimension, but '%d' dimensions are used.", name, expected_count,
               actual_count);
        free(info.filename);
    }

    enum DataType parent_type = type;
    type = symbol_to_type(entry);
    TRAVopt(VARDEC_EXPR(node));
    type = parent_type;
    return node;
}

node_st *CA_TCarrayvar(node_st *node)
{
    // Skip traversal of ARRAYVAR_DIMS as they are implicitly defined by the compiler and not the
    // use and are always of type int

    TRAVopt(ARRAYVAR_VAR(node));
    return node;
}

node_st *CA_TCglobaldec(node_st *node)
{
    enum DataType parent_type = type;
    type = GLOBALDEC_TYPE(node);

    TRAVopt(GLOBALDEC_VAR(node));
    type = parent_type;
    return node;
}

node_st *CA_TCvardec(node_st *node)
{
    node_st *var = VARDEC_VAR(node);
    char *name = VAR_NAME(var);
    enum ccn_nodetype var_type = NODE_TYPE(var);
    node_st *expr = VARDEC_EXPR(node);
    if (expr == NULL)
    {
        return node; // Nothing to check
    }
    enum ccn_nodetype expr_type = NODE_TYPE(expr);

    enum DataType parent_type = type;
    type = VARDEC_TYPE(node);

    if (var_type == NT_VAR)
    {
        if (expr_type == NT_ARRAYINIT)
        {
            struct ctinfo info = NODE_TO_CTINFO(node);
            info.filename = STRcpy(global.input_file);
            CTIobj(CTI_ERROR, true, info,
                   "Array expression can not be assigned to the variable '%s'.", name);
            free(info.filename);
        }
        else
        {
            TRAVopt(expr);
        }
    }
    else if (var_type == NT_ARRAYEXPR)
    {
        if (expr_type == NT_ARRAYINIT)
        {
            bool success = check_dimensions(ARRAYEXPR_DIMS(var), expr);
            if (success == false)
            {
                struct ctinfo info = NODE_TO_CTINFO(node);
                info.filename = STRcpy(global.input_file);
                CTIobj(CTI_ERROR, true, info,
                       "Array initalization does not match the dimension of the array.");
                free(info.filename);
            }
        }

        TRAVopt(expr);
    }
    else
    {
        release_assert(false);
    }

    type = parent_type;
    return node;
}
node_st *CA_TCfundec(node_st *node)
{
    return node;
}

node_st *CA_TCfundef(node_st *node)
{
    enum DataType parent_type = type;
    type = FUNHEADER_TYPE(FUNDEF_FUNHEADER(node));
    current = FUNDEF_SYMBOLS(node);
    TRAVopt(FUNDEF_FUNBODY(node));
    current = HTlookup(current, "@parent");
    release_assert(current != NULL);
    type = parent_type;
    return node;
}

node_st *CA_TCprogram(node_st *node)
{
    current = PROGRAM_SYMBOLS(node);
    TRAVopt(PROGRAM_DECLS(node));
    release_assert(type == DT_NULL);
    return node;
}
