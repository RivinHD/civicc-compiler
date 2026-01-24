#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "definitions.h"
#include "global/globals.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "to_string.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

htable_stptr current = NULL;
htable_stptr idx_table = NULL;
htable_stptr import_table = NULL;
htable_stptr constant_table = NULL;
enum DataType type = DT_NULL;
node_st *fundef = NULL;
uint32_t idx_counter = 0;
uint32_t fun_import_counter = 0;
uint32_t var_import_counter = 0;
uint32_t constant_counter = 0;
FILE *out_file = NULL;

/**
 * Helper functions.
 */
static void out(const char *restrict format, ...)
{
    va_list args;
    va_start(args, format);

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (global.output_buf == NULL)
    {
        release_assert(out_file != NULL);
        vfprintf(out_file, format, args);
    }
    else
    {
        vsnprintf(global.output_buf, global.output_buf_len, format, args);
    }
#else
    (void)format;
    (void)args;
#endif /* ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION */

    va_end(args);
}

static void inst0(const char *inst)
{
    out("    %s\n", inst);
}

static void inst1(const char *inst, ptrdiff_t index1)
{
    out("    %s %td\n", inst, index1);
}

static void inst2(const char *inst, ptrdiff_t index1, ptrdiff_t index2)
{
    out("    %s %td %td\n", inst, index1, index2);
}

static void instL(const char *inst, const char *label)
{
    out("    %s %s\n", inst, label);
}

static void label(const char *label)
{
    out("%s:\n", label);
}

static char *function_signature(node_st *funheader)
{
    release_assert(NODE_TYPE(funheader) == NT_FUNHEADER);
    char *name = VAR_NAME(FUNHEADER_VAR(funheader));
    char *ret_type = datatype_to_string(FUNHEADER_TYPE(funheader));
    char *output = STRfmt("\"%s\" %s", name, ret_type);
    free(ret_type);

    node_st *param = FUNHEADER_PARAMS(funheader);
    while (param != NULL)
    {
        char *old_output = output;
        char *str_type = datatype_to_string(PARAMS_TYPE(funheader));

        node_st *var = PARAMS_VAR(param);
        if (NODE_TYPE(var) == NT_ARRAYVAR)
        {
            output = STRfmt("%s %s[]", old_output, str_type);
        }
        else
        {
            output = STRfmt("%s %s", old_output, str_type);
        }

        free(old_output);
        free(str_type);
        param = PARAMS_NEXT(param);
    }
    return output;
}

static void exportfun(node_st *funheader, const char *label)
{
    char *output = function_signature(funheader);
    out(".exportfun %s %s", output, label);
    free(output);
}

static void importfun(node_st *funheader)
{
    char *output = function_signature(funheader);
    out(".importfun %s", output);
    free(output);
}

static void exportvar(const char *name, int32_t index)
{
    release_assert(index >= 0);
    out(".exportvar \"%s\" %d", name, index);
}

static void importvar(const char *name, node_st *entry)
{
    char *str_type = datatype_to_string(symbol_to_type(entry));
    node_st *var = get_var_from_symbol(entry);
    if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
    {
        out(".importvar \"%s\" %s[]", name, str_type);
    }
    else
    {
        out(".importvar \"%s\" %s", name, str_type);
    }
    free(str_type);
}

static char *int_to_str(int value)
{
    return STRfmt("%#x", value);
}

static char *float_to_str(double value)
{
    return STRfmt("%a", value);
}

static void consti(int value)
{
    char *str = int_to_str(value);
    out(".const int %s", str);
    free(str);
}

static void constf(double value)
{
    char *str = float_to_str(value);
    out(".const float %s", str);
    free(str);
}

static void constb(bool value)
{
    if (value)
    {
        out(".const float true");
    }
    else
    {
        out(".const float false");
    }
}

static void globalvar(node_st *entry)
{
    char *str_type = datatype_to_string(symbol_to_type(entry));
    node_st *var = get_var_from_symbol(entry);
    if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
    {
        out(".global %s[]", str_type);
    }
    else
    {
        out(".global %s", str_type);
    }
    free(str_type);
}

static void IDXinsert(htable_stptr table, char *key, ptrdiff_t index)
{
    HTinsert(table, key, (void *)index);
}

static ptrdiff_t IDXlookup(htable_stptr table, char *key)
{

    void *entry = HTlookup(table, key);
    release_assert(entry != NULL);
    return (ptrdiff_t)entry;
}

static ptrdiff_t IDXdeep_lookup(htable_stptr table, char *key)
{
    void *entry = deep_lookup(table, key);
    release_assert(entry != NULL);
    return (ptrdiff_t)entry;
}

/**
 * CodeGen Nodes
 */
node_st *CG_CGprogram(node_st *node)
{
    FILE *fd = NULL;
    if (global.output_buf == NULL && global.output_file != NULL)
    {
        fd = fopen(global.output_file, "w");
        out_file = fd;
        if (out_file == NULL)
        {
            CTI(CTI_ERROR, true, "Cannot write to file '%s'.", global.output_file);
            CTIabortOnError();
        }
    }
    else
    {
        release_assert(global.default_out_stream != NULL);
        out_file = global.default_out_stream;
    }

    import_table = HTnew_String(2 << 8);
    constant_table = HTnew_String(2 << 8);
    htable_stptr table = HTnew_String(2 << 8);
    idx_table = table;
    HTinsert(idx_table, "VARname", (void *)1);
    current = PROGRAM_SYMBOLS(node);
    TRAVchildren(node);
    HTdelete(table);
    HTdelete(import_table);
    HTdelete(constant_table);

    if (fd != NULL)
    {
        fclose(fd);
    }

    return node;
}

node_st *CG_CGdeclarations(node_st *node)
{
    TRAVchildren(node);
    return node;
}

node_st *CG_CGfundec(node_st *node)
{
    IDXinsert(import_table, VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(node))), fun_import_counter++);
    importfun(FUNDEC_FUNHEADER(node));

    TRAVchildren(node);
    return node;
}

node_st *CG_CGfundef(node_st *node)
{
    uint32_t parent_idx_counter = idx_counter;
    idx_counter = 0;
    htable_stptr table = HTnew_String(2 << 8);
    HTinsert(table, htable_parent_name, idx_table);
    idx_table = table;

    node_st *parent_fundef = fundef;
    htable_stptr parent_current = current;
    current = FUNDEF_SYMBOLS(node);
    fundef = node;

    char *fun_name = VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(node)));

    if (FUNDEF_HAS_EXPORT(node))
    {
        exportfun(FUNDEF_FUNHEADER(node), fun_name);
    }

    label(fun_name);

    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));

    HTdelete(table);
    idx_counter = parent_idx_counter;
    current = parent_current;
    fundef = parent_fundef;
    return node;
}

node_st *CG_CGglobaldec(node_st *node)
{
    char *globaldec_name = VAR_NAME(GLOBALDEC_VAR(node));
    IDXinsert(import_table, globaldec_name, var_import_counter++);
    importvar(globaldec_name, node);

    // Type not checkable because it is an extern var
    TRAVchildren(node);
    return node;
}

node_st *CG_CGglobaldef(node_st *node)
{
    globalvar(GLOBALDEF_VARDEC(node));

    // Type & Indextable is set by VarDec
    TRAVchildren(node);
    return node;
}

node_st *CG_CGfunheader(node_st *node)
{
    TRAVchildren(node);
    return node;
}

node_st *CG_CGparams(node_st *node)
{
    node_st *var = PARAMS_VAR(node);
    release_assert(NODE_TYPE(var) == NT_VAR || NODE_TYPE(var) == NT_ARRAYVAR);
    char *name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYVAR_VAR(var));
    IDXinsert(idx_table, name, idx_counter++);
    TRAVchildren(node);
    return node;
}

node_st *CG_CGfunbody(node_st *node)
{
    uint32_t vardec_count = 0;
    node_st *vardecs = FUNBODY_VARDECS(node);
    while (vardecs != NULL)
    {
        vardec_count++;
        vardecs = VARDECS_NEXT(vardecs);
    }

    inst1("esr", vardec_count);

    enum DataType parent_type = type;
    type = DT_void;
    TRAVchildren(node);
    type = parent_type;
    release_assert(idx_counter == vardec_count);
    return node;
}

node_st *CG_CGvardecs(node_st *node)
{
    TRAVchildren(node);
    return node;
}

node_st *CG_CGlocalfundefs(node_st *node)
{
    TRAVchildren(node);
    return node;
}

node_st *CG_CGstatements(node_st *node)
{
    TRAVchildren(node);
    return node;
}

node_st *CG_CGassign(node_st *node)
{
    int level = INT_MAX;
    char *name = VAR_NAME(ASSIGN_VAR(node));
    node_st *entry = deep_lookup_level(current, name, &level);
    enum DataType parent_type = type;
    type = symbol_to_type(entry);
    release_assert(type != DT_void);
    release_assert(type != DT_NULL);
    release_assert(level != INT_MAX);

    node_st *var = get_var_from_symbol(entry);
    node_st *expr = ASSIGN_EXPR(node);

    if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
    {
        release_assert(level <= 0); // Global or local variables
        release_assert(NODE_TYPE(expr) == NT_PROCCALL);
        release_assert(STReq(VAR_NAME(PROCCALL_VAR(expr)), alloc_func));
        node_st *proc_exprs = PROCCALL_EXPRS(expr);
        node_st *proc_expr = EXPRS_EXPR(proc_exprs);
        release_assert(EXPRS_NEXT(proc_exprs) == NULL);
        enum DataType assign_type = type;
        type = DT_int;
        TRAVopt(proc_expr);
        type = assign_type;
        switch (type)
        {
        case DT_int:
            inst0("inewa");
            break;
        case DT_float:
            inst0("fnewa");
            break;
        case DT_bool:
            inst0("bnewa");
            break;
        default:
            release_assert(false);
            break;
        }
    }

    if (level == 0)
    {
        ptrdiff_t index = IDXlookup(idx_table, name);
        // local variable
        if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
        {
            inst1("astore", index);
        }
        else
        {
            switch (type)
            {
            case DT_int:
                if (NODE_TYPE(expr) == NT_BINOP &&
                    (BINOP_OP(expr) == BO_add || BINOP_OP(expr) == BO_sub) &&
                    ((NODE_TYPE(BINOP_LEFT(expr)) == NT_VAR &&
                      NODE_TYPE(BINOP_RIGHT(expr)) == NT_INT &&
                      STReq(VAR_NAME(BINOP_LEFT(expr)), name)) ||
                     (NODE_TYPE(BINOP_RIGHT(expr)) == NT_VAR &&
                      NODE_TYPE(BINOP_LEFT(expr)) == NT_INT &&
                      STReq(VAR_NAME(BINOP_RIGHT(expr)), name))))
                {
                    // Increment / Decrement
                    node_st *int_node = NODE_TYPE(BINOP_RIGHT(expr)) == NT_INT ? BINOP_RIGHT(expr)
                                                                               : BINOP_LEFT(expr);
                    int val = INT_VAL(int_node);
                    if (val == 0)
                    {
                        // Do nothing
                    }
                    else if (val == 1)
                    {
                        if (BINOP_OP(expr) == BO_add)
                        {
                            inst1("iinc_1", index);
                        }
                        else
                        {
                            release_assert(BINOP_OP(expr) == BO_sub);
                            inst1("iidec_1", index);
                        }
                    }
                    else
                    {
                        TRAVopt(int_node);
                        ptrdiff_t const_index = IDXlookup(constant_table, int_to_str(val));
                        release_assert(val > 1);
                        if (BINOP_OP(expr) == BO_add)
                        {
                            inst2("iinc", index, const_index);
                        }
                        else
                        {
                            release_assert(BINOP_OP(expr) == BO_sub);
                            inst2("idec", index, const_index);
                        }
                    }

                    // Do not traverse as we already processed the complete assignment.
                    type = parent_type;
                    return node;
                }
                else
                {
                    inst1("istore", index);
                }
                break;
            case DT_float:
                inst1("fstore", index);
                break;
            case DT_bool:
                inst1("bstore", index);
                break;
            default:
                release_assert(false);
                break;
            }
        }
    }
    else if (level > 0)
    {
        // relative scope
        ptrdiff_t index = IDXdeep_lookup(idx_table, name);
        release_assert(NODE_TYPE(var) != NT_ARRAYEXPR);
        release_assert(NODE_TYPE(var) != NT_ARRAYVAR);

        switch (type)
        {
        case DT_int:
            inst2("istoren", level, index);
            break;
        case DT_float:
            inst2("fstoren", level, index);
            break;
        case DT_bool:
            inst2("bstoren", level, index);
            break;
        default:
            release_assert(false);
            break;
        }
    }
    else // level < 0
    {
        // global scope & import
        if (NODE_TYPE(entry) == NT_GLOBALDEC)
        {
            // import
            ptrdiff_t index = IDXlookup(import_table, name);
            if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
            {
                inst1("astoree", index);
            }
            else
            {
                switch (type)
                {
                case DT_int:
                    inst1("istoree", index);
                    break;
                case DT_float:
                    inst1("fstoree", index);
                    break;
                case DT_bool:
                    inst1("bstoree", index);
                    break;
                default:
                    release_assert(false);
                    break;
                }
            }
        }
        else
        {
            ptrdiff_t index = IDXdeep_lookup(idx_table, name);
            // global
            if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
            {
                inst1("astoreg", index);
            }
            else
            {
                switch (type)
                {
                case DT_int:
                    inst1("istoreg", index);
                    break;
                case DT_float:
                    inst1("fstoreg", index);
                    break;
                case DT_bool:
                    inst1("bstoreg", index);
                    break;
                default:
                    release_assert(false);
                    break;
                }
            }
        }
    }

    TRAVchildren(node);
    type = parent_type;
    return node;
}

node_st *CG_CGbinop(node_st *node)
{
    switch (BINOP_OP(node))
    {
    case BO_sub:
        switch (type)
        {
        case DT_int:
            inst0("isub");
            break;
        case DT_float:
            inst0("fsub");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_div:
        switch (type)
        {
        case DT_int:
            inst0("idiv");
            break;
        case DT_float:
            inst0("fdiv");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_add:
        switch (type)
        {
        case DT_int:
            inst0("iadd");
            break;
        case DT_float:
            inst0("fadd");
            break;
        case DT_bool:
            inst0("badd");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_mul:
        switch (type)
        {
        case DT_int:
            inst0("imul");
            break;
        case DT_float:
            inst0("fmul");
            break;
        case DT_bool:
            inst0("bmul");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_mod:
        switch (type)
        {
        case DT_int:
            inst0("irem");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_lt:
        switch (type)
        {
        case DT_int:
            inst0("ilt");
            break;
        case DT_float:
            inst0("flt");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_le:
        switch (type)
        {
        case DT_int:
            inst0("ile");
            break;
        case DT_float:
            inst0("fle");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_gt:
        switch (type)
        {
        case DT_int:
            inst0("igt");
            break;
        case DT_float:
            inst0("fgt");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_ge:
        switch (type)
        {
        case DT_int:
            inst0("ieq");
            break;
        case DT_float:
            inst0("feq");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_eq:
        switch (type)
        {
        case DT_int:
            inst0("ieq");
            break;
        case DT_float:
            inst0("feq");
            break;
        case DT_bool:
            inst0("beq");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_ne:
        switch (type)
        {
        case DT_int:
            inst0("ine");
            break;
        case DT_float:
            inst0("fne");
            break;
        case DT_bool:
            inst0("bne");
            break;
        default:
            release_assert(false);
            break;
        }
        break;
    case BO_and:
    case BO_or:
        release_assert(false); // All converted to Ternary Operators
        break;
    case BO_NULL:
        release_assert(false);
        break;
    }

    enum DataType parent_type = type;
    type = BINOP_ARGTYPE(node);
    release_assert(type != DT_void);
    release_assert(type != DT_NULL);
    TRAVchildren(node);
    type = parent_type;
    return node;
}

node_st *CG_CGmonop(node_st *node)
{
    switch (MONOP_OP(node))
    {
    case MO_not:
        release_assert(type == DT_bool);
        inst0("bnot");
        break;
    case MO_neg:
        switch (type)
        {
        case DT_int:
            inst0("ineg");
            break;
        case DT_float:
        default:
            inst0("fneg");
            break;
            release_assert(false);
            break;
        }
        break;
    case MO_NULL:
        release_assert(false);
        break;
    }

    TRAVchildren(node);
    return node;
}

node_st *CG_CGvardec(node_st *node)
{
    // TODO finish and beyond
    node_st *var = VARDEC_VAR(node);
    release_assert(NODE_TYPE(var) == NT_VAR || NODE_TYPE(var) == NT_ARRAYEXPR);
    char *name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYEXPR_VAR(var));
    IDXinsert(idx_table, name, idx_counter++);
    enum DataType parent_type = type;
    type = VARDEC_TYPE(node);
    release_assert(type != DT_void);
    release_assert(type != DT_NULL);
    TRAVchildren(node);
    type = parent_type;
    return node;
}

node_st *CG_CGproccall(node_st *node)
{
    node_st *var = PROCCALL_VAR(node);
    char *name = VAR_NAME(var);

    if (STReq(name, alloc_func))
    {
        // Already handled by assign
        return node;
    }

    node_st *entry = deep_lookup(current, name);
    enum DataType has_type = symbol_to_type(entry);
    if (type != DT_void)
    {
        release_assert(has_type != DT_NULL);
        release_assert(type == has_type);
    }

    release_assert(NODE_TYPE(entry) == NT_FUNDEF || NODE_TYPE(entry) == NT_FUNDEC);
    node_st *funheader =
        NODE_TYPE(entry) == NT_FUNDEF ? FUNDEF_FUNHEADER(entry) : FUNDEC_FUNHEADER(entry);
    node_st *exprs = PROCCALL_EXPRS(node);
    node_st *params = FUNHEADER_PARAMS(funheader);
    if (exprs != NULL)
    {
        while (exprs != NULL && params != NULL)
        {
            node_st *expr = EXPRS_EXPR(exprs);

            enum DataType parent_type = type;
            type = PARAMS_TYPE(params);
            release_assert(type != DT_void);
            release_assert(type != DT_NULL);
            TRAVopt(expr);
            type = parent_type;

            params = PARAMS_NEXT(params);
            exprs = EXPRS_NEXT(exprs);
        }
    }

    release_assert(exprs == NULL);
    release_assert(params == NULL);

    return node;
}

node_st *CG_CGexprs(node_st *node)
{
    TRAVchildren(node);
    return node;
}

node_st *CG_CGifstatement(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_bool;
    TRAVopt(IFSTATEMENT_EXPR(node));
    type = parent_type;
    TRAVopt(IFSTATEMENT_BLOCK(node));
    TRAVopt(IFSTATEMENT_ELSE_BLOCK(node));
    return node;
}

node_st *CG_CGwhileloop(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_bool;
    TRAVopt(WHILELOOP_EXPR(node));
    type = parent_type;
    TRAVopt(WHILELOOP_BLOCK(node));
    return node;
}

node_st *CG_CGdowhileloop(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_bool;
    TRAVopt(DOWHILELOOP_EXPR(node));
    type = parent_type;
    TRAVopt(WHILELOOP_BLOCK(node));
    return node;
}

node_st *CG_CGforloop(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_int;
    TRAVopt(FORLOOP_COND(node));
    TRAVopt(FORLOOP_ITER(node));
    TRAVopt(FORLOOP_ASSIGN(node));
    type = parent_type;
    TRAVopt(FORLOOP_BLOCK(node));
    return node;
}

node_st *CG_CGretstatement(node_st *node)
{
    enum DataType parent_type = type;
    release_assert(fundef != NULL);
    type = FUNHEADER_TYPE(FUNDEF_FUNHEADER(fundef));
    release_assert(type != DT_NULL);
    TRAVchildren(node);
    type = parent_type;
    return node;
}

node_st *CG_CGcast(node_st *node)
{
    enum DataType has_type = CAST_TYPE(node);
    release_assert(has_type != DT_NULL);
    release_assert(has_type != DT_void);
    release_assert(has_type == type);

    enum DataType parent_type = type;
    type = CAST_FROMTYPE(node);
    release_assert(type != DT_void);
    release_assert(type != DT_NULL);
    TRAVchildren(node);
    type = parent_type;
    return node;
}

node_st *CG_CGvar(node_st *node)
{
    node_st *entry = deep_lookup(current, VAR_NAME(node));
    enum DataType has_type = symbol_to_type(entry);
    release_assert(has_type != DT_NULL);
    release_assert(has_type != DT_void);
    release_assert(type == has_type);
    TRAVchildren(node);
    return node;
}

node_st *CG_CGarrayvar(node_st *node)
{
    TRAVchildren(node);
    return node;
}

node_st *CG_CGdimensionvars(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_int;
    TRAVchildren(node);
    type = parent_type;
    return node;
}

node_st *CG_CGarrayinit(node_st *node)
{
    // Does not exist anymore, was unrolled during CodeGenPrep
    release_assert(false);
    (void)node;
}

node_st *CG_CGarrayexpr(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_int;
    TRAVopt(ARRAYEXPR_DIMS(node));
    type = parent_type;
    TRAVopt(ARRAYEXPR_VAR(node));
    return node;
}

node_st *CG_CGarrayassign(node_st *node)
{
    enum DataType parent_type = type;
    char *name = VAR_NAME(ARRAYEXPR_VAR(ARRAYASSIGN_VAR(node)));
    node_st *entry = deep_lookup(current, name);
    enum DataType has_type = symbol_to_type(entry);
    release_assert(has_type != DT_NULL);
    release_assert(has_type != DT_void);
    type = has_type;
    TRAVchildren(node);
    type = parent_type;
    return node;
}

node_st *CG_CGint(node_st *node)
{
    release_assert(type == DT_int);
    TRAVchildren(node);
    return node;
}

node_st *CG_CGfloat(node_st *node)
{
    release_assert(type == DT_float);
    TRAVchildren(node);
    return node;
}

node_st *CG_CGbool(node_st *node)
{
    release_assert(type == DT_bool);
    TRAVchildren(node);
    return node;
}

node_st *CG_CGternary(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_bool;
    TRAVopt(TERNARY_PRED(node));
    type = parent_type;

    TRAVopt(TERNARY_PTRUE(node));
    TRAVopt(TERNARY_PFALSE(node));
    return node;
}
