#include "ccngen/ast.h"
#include "palm/ctinfo.h"
#include "palm/str.h"
#include "release_assert.h"
#include "to_string.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <string.h>

static htable_stptr current = NULL;
static bool anytype = false; // Allows an expression to set the type
static enum DataType type = DT_NULL;
static enum DataType rettype = DT_NULL;
static bool has_return = false;

static void reset_state()
{
    current = NULL;
    anytype = false;
    type = DT_NULL;
    rettype = DT_NULL;
    has_return = false;
}

static void type_check(node_st *node, const char *name, enum DataType expected, enum DataType value)
{
    release_assert(expected != DT_NULL);
    release_assert(value != DT_NULL);

    if (expected == value)
    {
        return;
    }

    struct ctinfo info = NODE_TO_CTINFO(node);
    char *expected_str = datatype_to_string(expected);
    char *value_str = datatype_to_string(value);
    const char *pretty_name = get_pretty_name(name);
    CTIobj(CTI_ERROR, true, info, "'%s' has type '%s' but expected type '%s'.", pretty_name,
           value_str, expected_str);
    free(expected_str);
    free(value_str);
}

static void binop_not_defined_on_type(node_st *node, enum DataType type)
{
    struct ctinfo info = NODE_TO_CTINFO(node);
    char *op_str = binoptype_to_string(BINOP_OP(node));
    release_assert(type != DT_NULL);
    char *type_str = datatype_to_string(type);
    CTIobj(CTI_ERROR, true, info, "The binop operation '%s' is not defined on the type '%s'.",
           op_str, type_str);
    free(op_str);
    free(type_str);
}

static bool check_nesting(unsigned int level, node_st *init)
{
    release_assert(init != NULL);

    if (level == 0)
    {
        // Last level should be an expression
        if (NODE_TYPE(init) == NT_ARRAYINIT)
        {
            struct ctinfo info = NODE_TO_CTINFO(init);
            CTIobj(CTI_ERROR, true, info, "Too many dimensions in array initalization.");
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
    CTIobj(CTI_ERROR, true, info,
           "Insufficient dimensions in array initalization, missing '%d' further dimensions.",
           level);

    return false;
}

static unsigned int count_dimension_vars(node_st *dimvars)
{
    release_assert(NODE_TYPE(dimvars) == NT_DIMENSIONVARS);
    unsigned int dims_count = 0;
    while (dimvars != NULL)
    {
        dims_count++;
        dimvars = DIMENSIONVARS_NEXT(dimvars);
    }
    return dims_count;
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

static bool check_dimensions(node_st *dims, node_st *array_init)
{
    release_assert(NODE_TYPE(dims) == NT_EXPRS || NODE_TYPE(dims) == NT_ARRAYVAR);
    release_assert(NODE_TYPE(array_init) == NT_ARRAYINIT);

    unsigned int exprs_count =
        NODE_TYPE(dims) == NT_EXPRS ? count_exprs(dims) : count_dimension_vars(dims);

    return check_nesting(exprs_count, array_init);
}

static void arrexpr_dimension_check(node_st *node, const char *name, node_st *expected,
                                    node_st *value)
{
    release_assert(NODE_TYPE(expected) == NT_ARRAYEXPR || NODE_TYPE(expected) == NT_ARRAYVAR);
    release_assert(NODE_TYPE(value) == NT_ARRAYEXPR || NODE_TYPE(value) == NT_ARRAYVAR);

    unsigned int expected_count = NODE_TYPE(expected) == NT_ARRAYEXPR
                                      ? count_exprs(ARRAYEXPR_DIMS(expected))
                                      : count_dimension_vars(ARRAYVAR_DIMS(expected));

    unsigned int actual_count = NODE_TYPE(value) == NT_ARRAYEXPR
                                    ? count_exprs(ARRAYEXPR_DIMS(value))
                                    : count_dimension_vars(ARRAYVAR_DIMS(value));

    if (expected_count != actual_count)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        const char *pretty_name = get_pretty_name(name);
        CTIobj(CTI_ERROR, true, info,
               "Array '%s' has '%d' dimension, but '%d' dimensions are used.", pretty_name,
               expected_count, actual_count);
    }
}

node_st *CA_TCbool(node_st *node)
{
    if (anytype)
    {
        release_assert(type == DT_NULL);
        type = DT_bool;
        anytype = false;
        return node;
    }
    else
    {
        char *str = BOOL_VAL(node) ? "true" : "false";
        type_check(node, str, type, DT_bool);
    }

    return node;
}

node_st *CA_TCfloat(node_st *node)
{
    if (anytype)
    {
        release_assert(type == DT_NULL);
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
        release_assert(type == DT_NULL);
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
        if (anytype)
        {
            type = DT_NULL;
            anytype = false;
        }
        return node;
    }
    release_assert(entry != NULL);

    node_st *var = get_var_from_symbol(entry);
    release_assert(var != NULL);
    if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        const char *pretty_name = get_pretty_name(name);
        CTIobj(CTI_ERROR, true, info, "Array '%s' can only be accessed with dimension indicies.",
               pretty_name);
    }

    enum DataType has_type = symbol_to_type(entry);
    if (anytype)
    {
        release_assert(type == DT_NULL);
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
    release_assert(has_type != DT_void);

    if (anytype)
    {
        release_assert(type == DT_NULL);
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
    if (type == DT_void)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        char *type_str = datatype_to_string(has_type);
        CTIobj(CTI_ERROR, true, info, "Cannot cast from type 'void' into type '%s'.", type_str);
        free(type_str);
    }
    CAST_FROMTYPE(node) = type;
    anytype = false;
    type = parent_type;
    return node;
}

node_st *CA_TCarrayexpr(node_st *node)
{
    node_st *var = ARRAYEXPR_VAR(node);
    char *name = VAR_NAME(var);
    node_st *entry = deep_lookup(current, name);
    if (entry == NULL && CTIgetErrors() > 0)
    {
        // Missing entry due to error, skip check.
        if (anytype)
        {
            type = DT_NULL;
            anytype = false;
        }
        return node;
    }
    release_assert(entry != NULL);

    node_st *defvar = get_var_from_symbol(entry);

    if (NODE_TYPE(defvar) != NT_ARRAYEXPR && NODE_TYPE(defvar) != NT_ARRAYVAR)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        const char *pretty_name = get_pretty_name(name);
        CTIobj(CTI_ERROR, true, info, "Scalar '%s' can not be accessed with an array expression.",
               pretty_name);
    }
    else
    {
        arrexpr_dimension_check(node, name, defvar, node);
    }

    // VAR is checked here, do not traverse. We can differentiate arrays with scalar this way.
    enum DataType has_type = symbol_to_type(entry);
    if (anytype)
    {
        release_assert(type == DT_NULL);
        type = has_type;
        anytype = false;
    }
    else
    {
        type_check(node, name, type, has_type);
    }

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
    bool was_infered = false;

    if (anytype)
    {
        // infer the type from the first argument
        release_assert(type == DT_NULL);
        TRAVopt(MONOP_LEFT(node));
        parent_type = type;
        was_infered = true;
        release_assert(anytype == false);

        if (type == DT_NULL && CTIgetErrors() > 0)
        {
            // Missing entry due to error, skip check.
            type = parent_type;
            return node;
        }
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

    if (!is_correct)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        char *op_str = monoptype_to_string(MONOP_OP(node));
        release_assert(type != DT_NULL);
        char *type_str = datatype_to_string(type);
        CTIobj(CTI_ERROR, true, info, "The monop operation '%s' is not defined on the type '%s'.",
               op_str, type_str);
        free(op_str);
        free(type_str);
    }

    if (!was_infered)
    {
        TRAVopt(MONOP_LEFT(node));
    }
    type = parent_type;
    return node;
}

node_st *CA_TCbinop(node_st *node)
{
    enum DataType parent_type = type;
    char *op_str = NULL;
    bool was_infered = false;

    if (type == DT_void)
    {
        binop_not_defined_on_type(node, type);
    }
    else
    {
        // Infer binop type
        switch (BINOP_OP(node))
        {
        case BO_add:
        case BO_sub:
        case BO_div:
        case BO_mul:
        case BO_mod:
            if (anytype)
            {
                // infer the type from the first argument
                release_assert(type == DT_NULL);
                TRAVopt(BINOP_LEFT(node));
                parent_type = type;
                was_infered = true;
                release_assert(anytype == false);

                if (type == DT_NULL && CTIgetErrors() > 0)
                {
                    // Missing entry due to error, skip check.
                    type = parent_type;
                    return node;
                }
            }
            break;
        case BO_lt:
        case BO_le:
        case BO_gt:
        case BO_ge:
        case BO_eq:
        case BO_ne:
            // Always yield a boolean
            if (anytype)
            {
                release_assert(type == DT_NULL);
                parent_type = DT_bool;
                anytype = false;
            }

            op_str = binoptype_to_string(BINOP_OP(node));
            type_check(node, op_str, parent_type, DT_bool);
            free(op_str);

            // infer the type from the first argument
            anytype = true;
            type = DT_NULL;
            TRAVopt(BINOP_LEFT(node));
            was_infered = true;
            release_assert(anytype == false);

            if (type == DT_NULL && CTIgetErrors() > 0)
            {
                // Missing entry due to error, skip check.
                type = parent_type;
                return node;
            }
            break;
        case BO_and:
        case BO_or:
            // Always yield a boolean
            if (anytype)
            {
                release_assert(type == DT_NULL);
                parent_type = DT_bool;
                anytype = false;
            }

            op_str = binoptype_to_string(BINOP_OP(node));
            type_check(node, op_str, parent_type, DT_bool);
            free(op_str);
            break;
        case BO_NULL:
            release_assert(false);
            break;
        }

        // Check valid bintop types
        switch (BINOP_OP(node))
        {
        case BO_sub:
        case BO_div:
            if ((type != DT_int) & (type != DT_float))
            {
                binop_not_defined_on_type(node, type);
            }
            break;
        case BO_add:
        case BO_mul:
            if ((type != DT_int) & (type != DT_float) & (type != DT_bool))
            {
                binop_not_defined_on_type(node, type);
            }
            break;
        case BO_mod:
            if (type != DT_int)
            {
                binop_not_defined_on_type(node, type);
            }
            break;
        case BO_lt:
        case BO_le:
        case BO_gt:
        case BO_ge:
            if ((type != DT_int) & (type != DT_float))
            {
                binop_not_defined_on_type(node, type);
            }
            break;
        case BO_eq:
        case BO_ne:
            if ((type != DT_int) & (type != DT_float) & (type != DT_bool))
            {
                binop_not_defined_on_type(node, type);
            }
            break;
        case BO_and:
        case BO_or:
            type = DT_bool; // Both arguments should be a bool
            break;
        case BO_NULL:
            release_assert(false);
            break;
        }
    }

    BINOP_ARGTYPE(node) = type;
    if (!was_infered)
    {
        TRAVopt(BINOP_LEFT(node));
    }
    TRAVopt(BINOP_RIGHT(node));

    type = parent_type;
    return node;
}

node_st *CA_TCretstatement(node_st *node)
{
    release_assert(anytype == false); // no anytype in statement
    enum DataType parent_type = type;
    type = rettype;
    node_st *expr = RETSTATEMENT_EXPR(node);
    if (expr == NULL)
    {
        if (type != DT_void)
        {
            struct ctinfo info = NODE_TO_CTINFO(node);
            CTIobj(CTI_ERROR, true, info, "Cannot return 'void' for none-void function.");
        }
    }
    else if (type == DT_void)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        CTIobj(CTI_ERROR, true, info, "Cannot return 'none-void' for void function.");
        TRAVopt(expr);
    }
    else
    {
        TRAVopt(expr);
    }

    type = parent_type;
    has_return = true;
    release_assert(anytype == false); // no anytype in statement
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
        if (anytype)
        {
            type = DT_NULL;
            anytype = false;
        }
        return node;
    }
    release_assert(entry != NULL);
    enum DataType has_type = symbol_to_type(entry);
    if (anytype)
    {
        release_assert(type == DT_NULL);
        type = has_type;
        anytype = false;
    }
    else
    {
        // only check type if called in a typed expression. A standalone procall in a function body
        // is of type void.
        if (type != DT_void)
        {
            type_check(var, name, type, has_type);
        }
    }

    release_assert(NODE_TYPE(entry) == NT_FUNDEF || NODE_TYPE(entry) == NT_FUNDEC);
    node_st *funheader =
        NODE_TYPE(entry) == NT_FUNDEF ? FUNDEF_FUNHEADER(entry) : FUNDEC_FUNHEADER(entry);
    unsigned int exprs_count = 0;
    unsigned int params_count = 0;
    node_st *exprs = PROCCALL_EXPRS(node);
    node_st *params = FUNHEADER_PARAMS(funheader);
    if (exprs != NULL)
    {
        while (exprs != NULL && params != NULL)
        {
            node_st *paramvar = PARAMS_VAR(params);
            node_st *expr = EXPRS_EXPR(exprs);
            if (NODE_TYPE(paramvar) == NT_ARRAYVAR)
            {
                if (NODE_TYPE(expr) == NT_VAR)
                {
                    node_st *exprentry = deep_lookup(current, VAR_NAME(expr));
                    if (exprentry == NULL && CTIgetErrors() > 0)
                    {
                        // Missing entry due to error, skip check.
                        params_count++;
                        exprs_count++;
                        params = PARAMS_NEXT(params);
                        exprs = EXPRS_NEXT(exprs);
                        continue;
                    }
                    release_assert(entry != NULL);

                    enum DataType exprtype = DT_NULL;
                    switch (NODE_TYPE(exprentry))
                    {
                    case NT_VARDEC:
                        exprtype = VARDEC_TYPE(exprentry);
                        break;
                    case NT_PARAMS:
                        exprtype = PARAMS_TYPE(exprentry);
                        break;
                    case NT_GLOBALDEC:
                        exprtype = GLOBALDEC_TYPE(exprentry);
                        break;
                    case NT_DIMENSIONVARS:
                        exprtype = DT_int;
                        break;
                    default:
                        release_assert(false);
                        break;
                    }
                    release_assert(exprtype != DT_NULL);

                    type_check(paramvar, VAR_NAME(expr), PARAMS_TYPE(params), exprtype);

                    node_st *exprvar = get_var_from_symbol(exprentry);
                    if (NODE_TYPE(exprvar) == NT_ARRAYVAR || NODE_TYPE(exprvar) == NT_ARRAYEXPR)
                    {
                        arrexpr_dimension_check(expr, VAR_NAME(expr), exprvar, paramvar);
                    }
                    else
                    {
                        struct ctinfo info = NODE_TO_CTINFO(expr);
                        CTIobj(CTI_ERROR, true, info,
                               "Expected array for argument '%s', but got scalar '%s'.",
                               get_pretty_name(VAR_NAME(ARRAYVAR_VAR(paramvar))),
                               get_pretty_name(VAR_NAME(expr)));
                    }
                }
                else
                {
                    struct ctinfo info = NODE_TO_CTINFO(expr);
                    CTIobj(CTI_ERROR, true, info,
                           "Expected array for argument '%s', but got scalar expression.",
                           get_pretty_name(VAR_NAME(ARRAYVAR_VAR(paramvar))));
                }
            }
            else
            {
                enum DataType parent_type = type;
                type = PARAMS_TYPE(params);
                TRAVopt(expr);
                type = parent_type;
            }

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
        CTIobj(CTI_ERROR, true, info, "Expected '%d' arguments, but only got '%d'.", params_count,
               exprs_count);
    }
    return node;
}
node_st *CA_TCdowhileloop(node_st *node)
{
    release_assert(anytype == false); // no anytype in statement
    enum DataType parent_type = type;
    type = DT_bool; // while loop should contain bool
    TRAVopt(DOWHILELOOP_EXPR(node));
    type = parent_type;
    TRAVopt(DOWHILELOOP_BLOCK(node));
    release_assert(anytype == false); // no anytype in statement
    return node;
}

node_st *CA_TCwhileloop(node_st *node)
{
    release_assert(anytype == false); // no anytype in statement
    enum DataType parent_type = type;
    bool parent_has_return = has_return;
    type = DT_bool; // while loop should contain bool
    TRAVopt(WHILELOOP_EXPR(node));
    type = parent_type;
    TRAVopt(WHILELOOP_BLOCK(node));
    has_return = parent_has_return;   // While is not guaranteed to be executed
    release_assert(anytype == false); // no anytype in statement
    return node;
}

node_st *CA_TCifstatement(node_st *node)
{
    release_assert(anytype == false); // no anytype in statement
    enum DataType parent_type = type;
    bool parent_has_return = has_return;
    type = DT_bool; // if statement should contain bool
    TRAVopt(IFSTATEMENT_EXPR(node));
    type = parent_type;
    has_return = false;
    TRAVopt(IFSTATEMENT_BLOCK(node));
    bool if_has_return = has_return;
    has_return = false; // Reset for else check
    TRAVopt(IFSTATEMENT_ELSE_BLOCK(node));

    // we already have a return or if AND else branch have a return;
    has_return = parent_has_return | (if_has_return & has_return);
    release_assert(anytype == false); // no anytype in statement
    return node;
}

node_st *CA_TCforloop(node_st *node)
{
    release_assert(anytype == false); // no anytype in statement
    enum DataType parent_type = type;
    bool parent_has_return = has_return;
    type = DT_int; // for loop should only contain integer
    TRAVopt(FORLOOP_COND(node));
    TRAVopt(FORLOOP_ITER(node));
    TRAVopt(ASSIGN_EXPR(FORLOOP_ASSIGN(node))); // No need to check the assign var
    type = parent_type;
    TRAVopt(FORLOOP_BLOCK(node));
    has_return = parent_has_return;   // For loop is not guaranteed to be executed
    release_assert(anytype == false); // no anytype in statement
    return node;
}

node_st *CA_TCassign(node_st *node)
{
    release_assert(anytype == false); // no anytype in statement
    node_st *entry = deep_lookup(current, VAR_NAME(ASSIGN_VAR(node)));
    if (entry == NULL && CTIgetErrors() > 0)
    {
        // Missing entry due to error, skip check.
        return node;
    }
    release_assert(entry != NULL);

    node_st *var = get_var_from_symbol(entry);
    release_assert(var != NULL);

    if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
    {
        char *name = NODE_TYPE(var) == NT_ARRAYEXPR ? VAR_NAME(ARRAYEXPR_VAR(var))
                                                    : VAR_NAME(ARRAYVAR_VAR(var));
        struct ctinfo info = NODE_TO_CTINFO(node);
        const char *pretty_name = get_pretty_name(name);
        CTIobj(CTI_ERROR, true, info,
               "Array '%s' can not be assigned a scalar value without providing dimension "
               "indices.",
               pretty_name);
        return node;
    }

    const char *name = VAR_NAME(ASSIGN_VAR(node));
    if (STRprefix("@for", name))
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        const char *pretty_name = get_pretty_name(name);
        CTIobj(CTI_ERROR, true, info, "Assignment to for loop iterator '%s' is illegal.",
               pretty_name);
        return node;
    }

    enum DataType parent_type = type;
    type = symbol_to_type(entry);
    TRAVopt(ASSIGN_EXPR(node));
    type = parent_type;
    release_assert(anytype == false); // no anytype in statement
    return node;
}

node_st *CA_TCarrayassign(node_st *node)
{
    release_assert(anytype == false); // no anytype in statement
    char *name = VAR_NAME(ARRAYEXPR_VAR(ARRAYASSIGN_VAR(node)));
    node_st *entry = deep_lookup(current, name);
    if (entry == NULL && CTIgetErrors() > 0)
    {
        // Missing entry due to error, skip check.
        return node;
    }
    release_assert(entry != NULL);

    node_st *var = get_var_from_symbol(entry);
    release_assert(var != NULL);

    if (NODE_TYPE(var) == NT_VAR || NODE_TYPE(var) == NT_DIMENSIONVARS)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        const char *pretty_name = get_pretty_name(VAR_NAME(var));
        CTIobj(CTI_ERROR, true, info, "Can not use scalar variable '%s' as array variable.",
               pretty_name);
        return node;
    }

    release_assert(NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR);
    arrexpr_dimension_check(node, name, var, ARRAYASSIGN_VAR(node));

    enum DataType parent_type = type;
    type = DT_int;
    TRAVopt(ARRAYEXPR_DIMS(ARRAYASSIGN_VAR(node)));

    type = symbol_to_type(entry);
    TRAVopt(ARRAYASSIGN_EXPR(node));
    type = parent_type;
    release_assert(anytype == false); // no anytype in statement
    return node;
}

node_st *CA_TCarrayvar(node_st *node)
{
    // Skip traversal of ARRAYVAR_DIMS as they are implicitly defined by the compiler and not
    // the use and are always of type int

    TRAVopt(ARRAYVAR_VAR(node));
    return node;
}

node_st *CA_TCglobaldec(node_st *node)
{
    // Global dec are extern, we can not check them
    return node;
}

node_st *CA_TCvardec(node_st *node)
{
    enum DataType parent_type = type;

    node_st *var = VARDEC_VAR(node);
    if (NODE_TYPE(var) == NT_ARRAYEXPR)
    {
        type = DT_int;
        TRAVopt(ARRAYEXPR_DIMS(var));
    }

    type = VARDEC_TYPE(node);

    char *name = VAR_NAME(NODE_TYPE(var) == NT_ARRAYEXPR ? ARRAYEXPR_VAR(var) : var);
    enum ccn_nodetype var_type = NODE_TYPE(var);
    node_st *expr = VARDEC_EXPR(node);
    if (expr == NULL)
    {
        type = parent_type;
        return node; // Nothing to check
    }

    enum ccn_nodetype expr_type = NODE_TYPE(expr);
    if (var_type == NT_VAR)
    {
        if (expr_type == NT_ARRAYINIT)
        {
            struct ctinfo info = NODE_TO_CTINFO(node);
            const char *pretty_name = get_pretty_name(name);
            CTIobj(CTI_ERROR, true, info,
                   "Array expression can not be assigned to the variable '%s'.", pretty_name);
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
                CTIobj(CTI_ERROR, true, info,
                       "Array initalization does not match the dimension of the array.");
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
    // Fundec are extern, we can not check them
    return node;
}

node_st *CA_TCfundef(node_st *node)
{
    htable_stptr parent_current = current;
    enum DataType parent_type = type;
    enum DataType parent_rettype = rettype;
    bool parent_has_return = has_return;
    type = DT_void;
    has_return = false;
    rettype = FUNHEADER_TYPE(FUNDEF_FUNHEADER(node));
    current = FUNDEF_SYMBOLS(node);
    release_assert(current != NULL);

    // FUNHEADER does not need to be type checked
    TRAVopt(FUNDEF_FUNBODY(node));

    if (!has_return && rettype != DT_void)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        CTIobj(CTI_ERROR, true, info,
               "non-void function does not return a value in all control paths.");
    }

    type = parent_type;
    rettype = parent_rettype;
    has_return = parent_has_return;
    current = parent_current;
    release_assert(current != NULL);
    return node;
}

node_st *CA_TCprogram(node_st *node)
{
    reset_state();
    current = PROGRAM_SYMBOLS(node);
    TRAVopt(PROGRAM_DECLS(node));
    release_assert(type == DT_NULL);
    release_assert(rettype == DT_NULL);
    release_assert(has_return == false);
    release_assert(anytype == false);
    check_phase_error();
    return node;
}
