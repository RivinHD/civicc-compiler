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
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static htable_stptr current = NULL;
static htable_stptr index_table = NULL;
static htable_stptr import_table = NULL;
static htable_stptr constant_table = NULL;
static enum DataType type = DT_NULL;
static node_st *fundef = NULL;
static uint32_t idx_counter = 0;
static uint32_t fun_import_counter = 0;
static uint32_t var_import_counter = 0;
static uint32_t constant_counter = 0;
static FILE *out_file = NULL;
static uint32_t if_counter = 0;
static uint32_t loop_counter = 0;
static bool is_expr = true;
static bool is_arrayexpr_store = false;
static uint32_t lfun_counter = 0;

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
        if (out_file != NULL)
        {
            vfprintf(out_file, format, args);
        }
    }
    else
    {
        release_assert(global.output_buf_len != 0);
        int added = vsnprintf(global.output_buf, global.output_buf_len, format, args);
        if (added > 0)
        {
            if ((uint32_t)added > global.output_buf_len)
            {
                // Added more characters than buffer can hold
                release_assert(false);
            }
            else
            {
                global.output_buf += added;
                global.output_buf_len -= (uint32_t)added;
            }
        }
        else
        {
            // Failed to add characters to buffer
            release_assert(false);
        }
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

static void instjsr(uint32_t count, const char *label)
{
    out("    jsr %d %s\n", count, label);
}

static void label(const char *label)
{
    out("%s:\n", label);
}

static char *function_signature(node_st *funheader)
{
    release_assert(NODE_TYPE(funheader) == NT_FUNHEADER);
    const char *name = get_pretty_name(VAR_NAME(FUNHEADER_VAR(funheader)));
    char *ret_type = datatype_to_string(FUNHEADER_TYPE(funheader));
    char *output = STRfmt("\"%s\" %s", name, ret_type);
    free(ret_type);

    node_st *param = FUNHEADER_PARAMS(funheader);
    while (param != NULL)
    {
        char *old_output = output;
        char *str_type = datatype_to_string(PARAMS_TYPE(param));

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
    out(".exportfun %s %s\n", output, label);
    free(output);
}

static void importfun(node_st *funheader)
{
    char *output = function_signature(funheader);
    out(".importfun %s\n", output);
    free(output);
}

static void exportvar(const char *name, ptrdiff_t index)
{
    release_assert(index >= 0);
    out(".exportvar \"%s\" %d\n", name, index);
}

static void importvar(const char *name, node_st *entry)
{
    char *str_type = datatype_to_string(symbol_to_type(entry));
    node_st *var = get_var_from_symbol(entry);
    if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
    {
        out(".importvar \"%s\" %s[]\n", name, str_type);
    }
    else
    {
        out(".importvar \"%s\" %s\n", name, str_type);
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
    out(".const int %s  ; %d\n", str, value);
    free(str);
}

static void constf(double value)
{
    char *str = float_to_str(value);
    out(".const float %s  ; %e\n", str, value);
    free(str);
}

static void constb(bool value)
{
    if (value)
    {
        out(".const float true\n");
    }
    else
    {
        out(".const float false\n");
    }
}

static void globalvar(node_st *entry)
{
    char *str_type = datatype_to_string(symbol_to_type(entry));
    node_st *var = get_var_from_symbol(entry);
    if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
    {
        out(".global %s[]\n", str_type);
    }
    else
    {
        out(".global %s\n", str_type);
    }
    free(str_type);
}

static void IDXinsert(htable_stptr table, char *key, ptrdiff_t index)
{
    HTinsert(table, key, (void *)(index + 1));
}

static ptrdiff_t IDXlookup(htable_stptr table, char *key)
{

    void *entry = HTlookup(table, key);
    release_assert(entry != NULL);
    return (ptrdiff_t)entry - 1;
}

static ptrdiff_t IDXdeep_lookup(htable_stptr table, char *key)
{
    void *entry = deep_lookup(table, key);
    release_assert(entry != NULL);
    return (ptrdiff_t)entry - 1;
}

/**
 * CodeGen Nodes
 */
node_st *CG_CGprogram(node_st *node)
{
    char *str = node_to_string(node);
    printf("%s", str);
    free(str);

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
        out_file = global.default_out_stream;
    }

    import_table = HTnew_String(2 << 8);
    constant_table = HTnew_String(2 << 8);
    htable_stptr table = HTnew_String(2 << 8);
    index_table = table;
    current = PROGRAM_SYMBOLS(node);

    TRAVchildren(node);

    HTdelete(table);
    HTdelete(import_table);
    for (htable_iter_st *iter = HTiterate(constant_table); iter; iter = HTiterateNext(iter))
    {
        // Getter functions to extract htable elements
        char *key = HTiterKey(iter);
        free(key);
    }
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
    IDXinsert(import_table, VAR_NAME(FUNHEADER_VAR(FUNDEC_FUNHEADER(node))), fun_import_counter++);
    importfun(FUNDEC_FUNHEADER(node));
    return node;
}

node_st *CG_CGfundef(node_st *node)
{
    char *fun_name = VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(node)));
    if (fundef != NULL)
    {
        // only counter for local functions
        char *old_fun_name = fun_name;
        fun_name = STRfmt("@fun_lf%d_%s", lfun_counter++, get_pretty_name(old_fun_name));
        VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(node))) = fun_name;
        free(old_fun_name);
    }

    uint32_t parent_idx_counter = idx_counter;
    idx_counter = 0;
    htable_stptr table = HTnew_String(2 << 8);
    HTinsert(table, htable_parent_name, index_table);
    index_table = table;

    node_st *parent_fundef = fundef;
    htable_stptr parent_current = current;
    current = FUNDEF_SYMBOLS(node);
    fundef = node;

    if (FUNDEF_HAS_EXPORT(node))
    {
        exportfun(FUNDEF_FUNHEADER(node), get_pretty_name(fun_name));
    }

    label(get_pretty_name(fun_name));

    bool parent_is_expr = is_expr;
    is_expr = false;
    TRAVopt(FUNDEF_FUNHEADER(node));

    uint32_t vardec_count = 0;
    node_st *vardecs = FUNBODY_VARDECS(FUNDEF_FUNBODY(node));
    while (vardecs != NULL)
    {
        vardec_count++;
        vardecs = VARDECS_NEXT(vardecs);
    }

    if (vardec_count != 0)
    {
        inst1("esr", vardec_count);
    }

    is_expr = parent_is_expr;
    TRAVopt(FUNDEF_FUNBODY(node));

    if (FUNHEADER_TYPE(FUNDEF_FUNHEADER(node)) == DT_void)
    {
        inst0("return");
    }

    uint32_t params_count = 0;
    node_st *params = FUNHEADER_PARAMS(FUNDEF_FUNHEADER(node));
    while (params != NULL)
    {
        params_count++;
        params = PARAMS_NEXT(params);
    }

    release_assert(idx_counter == vardec_count + params_count);

    idx_counter = parent_idx_counter;
    current = parent_current;
    fundef = parent_fundef;
    index_table = HTlookup(index_table, htable_parent_name);
    release_assert(index_table != NULL);
    HTdelete(table);
    return node;
}

node_st *CG_CGglobaldec(node_st *node)
{
    node_st *var = GLOBALDEC_VAR(node);
    char *globaldec_name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYVAR_VAR(var));
    IDXinsert(import_table, globaldec_name, var_import_counter++);
    importvar(globaldec_name, node);
    return node;
}

node_st *CG_CGglobaldef(node_st *node)
{
    globalvar(GLOBALDEF_VARDEC(node));

    // Type & Indextable is set by VarDec
    TRAVchildren(node);

    if (GLOBALDEF_HAS_EXPORT(node) == true)
    {
        node_st *var = GLOBALDEF_VARDEC(node);
        char *name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYEXPR_VAR(var));
        ptrdiff_t idx = IDXlookup(index_table, name);
        exportvar(name, idx);
    }
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
    IDXinsert(index_table, name, idx_counter++);
    bool parent_is_expr = is_expr;
    is_expr = false;
    TRAVchildren(node);
    is_expr = parent_is_expr;
    return node;
}

node_st *CG_CGfunbody(node_st *node)
{
    enum DataType parent_type = type;
    type = DT_void;
    TRAVopt(FUNBODY_VARDECS(node));
    TRAVopt(FUNBODY_STMTS(node));
    TRAVopt(FUNBODY_LOCALFUNDEFS(node));
    type = parent_type;
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
        ptrdiff_t index = IDXlookup(index_table, name);
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
                            inst1("idec_1", index);
                        }
                    }
                    else
                    {
                        bool parent_is_expr = is_expr;
                        is_expr = false;
                        TRAVopt(int_node); // Adds int node to table if not present
                        is_expr = parent_is_expr;

                        char *val_str = int_to_str(val);
                        ptrdiff_t const_index = IDXlookup(constant_table, val_str);
                        free(val_str);
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
                    TRAVopt(expr);
                    inst1("istore", index);
                }
                break;
            case DT_float:
                TRAVopt(expr);
                inst1("fstore", index);
                break;
            case DT_bool:
                TRAVopt(expr);
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
        ptrdiff_t index = IDXdeep_lookup(index_table, name);
        release_assert(NODE_TYPE(var) != NT_ARRAYEXPR);
        release_assert(NODE_TYPE(var) != NT_ARRAYVAR);

        TRAVopt(expr);
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
                TRAVopt(expr);
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
            ptrdiff_t index = IDXdeep_lookup(index_table, name);
            // global
            if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
            {
                inst1("astoreg", index);
            }
            else
            {
                TRAVopt(expr);
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

    type = parent_type;
    return node;
}

node_st *CG_CGbinop(node_st *node)
{
    enum DataType parent_type = type;
    type = BINOP_ARGTYPE(node);
    release_assert(type != DT_void);
    release_assert(type != DT_NULL);
    TRAVchildren(node);

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
            if (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT && INT_VAL(BINOP_RIGHT(node)) == 0)
            {
                struct ctinfo info = NODE_TO_CTINFO(node);
                info.filename = STRcpy(global.input_file);
                CTIobj(CTI_WARN, true, info, "Division by zero.");
                free(info.filename);
            }
            inst0("idiv");
            break;
        case DT_float:
            if (NODE_TYPE(BINOP_RIGHT(node)) == NT_FLOAT && FLOAT_VAL(BINOP_RIGHT(node)) == 0.0)
            {
                struct ctinfo info = NODE_TO_CTINFO(node);
                info.filename = STRcpy(global.input_file);
                CTIobj(CTI_WARN, true, info, "Division by zero.");
                free(info.filename);
            }
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
            inst0("ige");
            break;
        case DT_float:
            inst0("fge");
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
    type = parent_type;

    return node;
}

node_st *CG_CGmonop(node_st *node)
{
    TRAVchildren(node);

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

    return node;
}

node_st *CG_CGvardec(node_st *node)
{
    node_st *var = VARDEC_VAR(node);
    release_assert(NODE_TYPE(var) == NT_VAR || NODE_TYPE(var) == NT_ARRAYEXPR);
    char *name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYEXPR_VAR(var));
    IDXinsert(index_table, name, idx_counter++);

    enum DataType parent_type = type;
    type = VARDEC_TYPE(node);
    release_assert(type != DT_void);
    release_assert(type != DT_NULL);
    bool parent_is_expr = is_expr;
    is_expr = false;
    TRAVopt(VARDEC_VAR(node));
    is_expr = parent_is_expr;
    TRAVopt(VARDEC_EXPR(node));

    if (VARDEC_EXPR(node) != NULL)
    {
        release_assert(NODE_TYPE(var) == NT_VAR);
        ptrdiff_t index = IDXlookup(index_table, name);
        switch (type)
        {
        case DT_NULL:
        case DT_void:
            release_assert(false);
            break;
        case DT_int:
            inst1("istore", index);
            break;
        case DT_float:
            inst1("fstore", index);
            break;
        case DT_bool:
            inst1("bstore", index);
            break;
        }
    }

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

    int level = INT_MAX;
    node_st *entry = deep_lookup_level(current, name, &level);
    enum DataType has_type = symbol_to_type(entry);
    if (type != DT_void)
    {
        release_assert(has_type != DT_NULL);
        release_assert(type != DT_NULL);
        release_assert(type == has_type);
    }
    release_assert(level != INT_MAX);

    if (level == 0)
    {
        inst0("isrl");
    }
    else if (level > 0)
    {
        if (level == 1)
        {
            inst0("isr");
        }
        else
        {
            inst1("isrn", level - 1);
        }
    }
    else // level < 0
    {
        inst0("isrg");
    }

    release_assert(NODE_TYPE(entry) == NT_FUNDEF || NODE_TYPE(entry) == NT_FUNDEC);
    node_st *funheader =
        NODE_TYPE(entry) == NT_FUNDEF ? FUNDEF_FUNHEADER(entry) : FUNDEC_FUNHEADER(entry);
    node_st *exprs = PROCCALL_EXPRS(node);
    node_st *params = FUNHEADER_PARAMS(funheader);
    uint32_t exprs_count = 0;
    while (exprs != NULL && params != NULL)
    {
        exprs_count += 1;
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

    release_assert(exprs == NULL);
    release_assert(params == NULL);

    char *fun_name = VAR_NAME(FUNHEADER_VAR(funheader));
    if (NODE_TYPE(entry) == NT_FUNDEC)
    {
        inst1("jsre", IDXlookup(import_table, fun_name));
    }
    else
    {
        instjsr(exprs_count, get_pretty_name(fun_name));
    }

    // edge case procall to type == DT_void we must omit the return value which is placed
    // ontop of the stack by using the '<t>pop' instruction
    if (type == DT_void)
    {
        switch (has_type)
        {
        case DT_NULL:
            release_assert(false);
            break;
        case DT_void:
            // Nothing to do
            break;
        case DT_bool:
            inst0("bpop");
            break;
        case DT_int:
            inst0("ipop");
            break;
        case DT_float:
            inst0("fpop");
            break;
        }
    }

    return node;
}

node_st *CG_CGexprs(node_st *node)
{
    TRAVchildren(node);
    return node;
}

node_st *CG_CGifstatement(node_st *node)
{
    char *else_label = STRfmt("else%d", if_counter);
    char *end_label = STRfmt("ifend%d", if_counter);
    if_counter++;

    enum DataType parent_type = type;
    type = DT_bool;
    TRAVopt(IFSTATEMENT_EXPR(node));
    type = parent_type;

    if (IFSTATEMENT_BLOCK(node) != NULL && IFSTATEMENT_ELSE_BLOCK(node) != NULL)
    {
        instL("branch_f", else_label);
        TRAVopt(IFSTATEMENT_BLOCK(node));
        instL("jump", end_label);
        label(else_label);
        TRAVopt(IFSTATEMENT_ELSE_BLOCK(node));
        label(end_label);
    }
    else if (IFSTATEMENT_BLOCK(node) == NULL)
    {
        instL("branch_t", end_label);
        TRAVopt(IFSTATEMENT_ELSE_BLOCK(node));
        label(end_label);
    }
    else if (IFSTATEMENT_ELSE_BLOCK(node) == NULL)
    {
        instL("branch_f", end_label);
        TRAVopt(IFSTATEMENT_BLOCK(node));
        label(end_label);
    }

    free(else_label);
    free(end_label);
    return node;
}

node_st *CG_CGwhileloop(node_st *node)
{
    char *while_label = STRfmt("while%d", loop_counter);
    char *end_label = STRfmt("whileend%d", loop_counter);
    loop_counter++;

    enum DataType parent_type = type;
    type = DT_bool;
    label(while_label);
    TRAVopt(WHILELOOP_EXPR(node));
    instL("branch_f", end_label);
    type = parent_type;

    TRAVopt(WHILELOOP_BLOCK(node));
    instL("jump", while_label);
    label(end_label);

    free(while_label);
    free(end_label);
    return node;
}

node_st *CG_CGdowhileloop(node_st *node)
{
    char *while_label = STRfmt("while%d", loop_counter);
    loop_counter++;

    label(while_label);
    TRAVopt(DOWHILELOOP_BLOCK(node));

    enum DataType parent_type = type;
    type = DT_bool;
    TRAVopt(DOWHILELOOP_EXPR(node));
    type = parent_type;
    instL("branch_t", while_label);

    free(while_label);
    return node;
}

node_st *CG_CGforloop(node_st *node)
{
    node_st *iter = FORLOOP_ITER(node);
    if (iter != NULL && NODE_TYPE(iter) == NT_INT && INT_VAL(iter) == 0)
    {
        struct ctinfo info = NODE_TO_CTINFO(iter);
        info.filename = STRcpy(global.input_file);
        CTIobj(CTI_WARN, true, info, "Step is '0' and may lead to undefined behaviour.");
        free(info.filename);
    }

    node_st *assign = FORLOOP_ASSIGN(node);
    release_assert(NODE_TYPE(ASSIGN_EXPR(assign)) == NT_VAR ||
                   NODE_TYPE(ASSIGN_EXPR(assign)) == NT_INT);
    release_assert(NODE_TYPE(FORLOOP_COND(node)) == NT_VAR ||
                   NODE_TYPE(FORLOOP_COND(node)) == NT_INT);
    if (FORLOOP_ITER(node) != NULL)
    {
        release_assert(NODE_TYPE(FORLOOP_ITER(node)) == NT_VAR ||
                       NODE_TYPE(FORLOOP_ITER(node)) == NT_INT);
    }

    enum DataType parent_type = type;
    type = DT_int;
    char *start_label = STRfmt("for%d", loop_counter);
    char *end_label = STRfmt("endfor%d", loop_counter);
    loop_counter++;
    char *assign_name = VAR_NAME(ASSIGN_VAR(assign));

    TRAVopt(assign);

    if (iter == NULL || (NODE_TYPE(iter) == NT_INT && INT_VAL(iter) == 1))
    {
        label(start_label);
        TRAVopt(ASSIGN_VAR(assign));
        TRAVopt(FORLOOP_COND(node));
        inst0("ilt");
        instL("branch_f", end_label);
        type = parent_type;
        TRAVopt(FORLOOP_BLOCK(node));
        type = DT_int;
        TRAVopt(iter);
        type = parent_type;
        inst1("iinc_1", IDXlookup(index_table, assign_name));
        instL("jump", start_label);
        label(end_label);
    }
    else if (NODE_TYPE(iter) == NT_INT)
    {
        label(start_label);
        TRAVopt(ASSIGN_VAR(assign));
        TRAVopt(FORLOOP_COND(node));
        inst0("ilt");
        instL("branch_f", end_label);
        type = parent_type;
        TRAVopt(FORLOOP_BLOCK(node));
        bool parent_is_expr = is_expr;
        is_expr = false;
        type = DT_int;
        TRAVopt(iter); // only add to constant table if not available
        type = parent_type;
        is_expr = parent_is_expr;
        char *val_str = int_to_str(INT_VAL(iter));
        inst2("iinc", IDXlookup(index_table, assign_name), IDXlookup(constant_table, val_str));
        free(val_str);
        instL("jump", start_label);
        label(end_label);
    }
    else
    {
        // general case
        release_assert(NODE_TYPE(iter) == NT_VAR);
        release_assert(NODE_TYPE(FORLOOP_COND(node)) == NT_VAR);
        ptrdiff_t cond_idx = IDXlookup(index_table, VAR_NAME(FORLOOP_COND(node)));

        TRAVopt(FORLOOP_COND(node));
        TRAVopt(ASSIGN_VAR(assign));
        inst0("isub");
        TRAVopt(iter);
        inst0("idiv");
        inst1("istore", cond_idx);
        label(start_label);
        TRAVopt(iter);
        inst0("iloadc_0");
        inst0("igt");
        instL("branch_f", end_label);
        type = parent_type;
        TRAVopt(FORLOOP_BLOCK(node));
        type = DT_int;
        inst1("idec_1", cond_idx);
        TRAVopt(ASSIGN_VAR(assign));
        TRAVopt(iter);
        type = parent_type;
        inst0("iadd");
        inst1("istore", IDXlookup(index_table, VAR_NAME(iter)));
        instL("jump", start_label);
        label(end_label);
    }

    free(start_label);
    free(end_label);
    return node;
}

node_st *CG_CGretstatement(node_st *node)
{
    enum DataType parent_type = type;
    release_assert(fundef != NULL);
    type = FUNHEADER_TYPE(FUNDEF_FUNHEADER(fundef));
    release_assert(type != DT_NULL);
    TRAVchildren(node);
    switch (type)
    {
    case DT_NULL:
        release_assert(false);
        break;
    case DT_void:
        inst0("return");
        break;
    case DT_bool:
        inst0("breturn");
        break;
    case DT_int:
        inst0("ireturn");
        break;
    case DT_float:
        inst0("freturn");
        break;
    }
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
    if (has_type == DT_int && type == DT_float)
    {
        inst0("f2i");
    }
    else if (has_type == DT_float && type == DT_int)
    {
        inst0("i2f");
    }
    else
    {
        release_assert(false);
    }
    type = parent_type;
    return node;
}

node_st *CG_CGvar(node_st *node)
{
    if (is_expr == false)
    {
        return node;
    }

    int level = INT_MAX;
    node_st *entry = deep_lookup_level(current, VAR_NAME(node), &level);
    enum DataType has_type = symbol_to_type(entry);
    release_assert(type != DT_NULL);
    release_assert(has_type != DT_NULL);
    release_assert(has_type != DT_void);
    release_assert(type == has_type);
    release_assert(level != INT_MAX);

    char type_str = '\0';
    switch (has_type)
    {
    case DT_NULL:
    case DT_void:
        release_assert(false);
        break;
    case DT_bool:
        type_str = 'b';
        break;
    case DT_int:
        type_str = 'i';
        break;
    case DT_float:
        type_str = 'f';
        break;
    }

    node_st *var = get_var_from_symbol(entry);
    if (NODE_TYPE(var) == NT_ARRAYEXPR || NODE_TYPE(var) == NT_ARRAYVAR)
    {
        type_str = 'a';
    }

    if (level == 0)
    {
        ptrdiff_t idx = IDXdeep_lookup(index_table, VAR_NAME(node));

        char load_str = '\0';
        switch (idx)
        {
        case 0:
            load_str = '0';
            break;
        case 1:
            load_str = '1';
            break;
        case 2:
            load_str = '2';
            break;
        case 3:
            load_str = '3';
            break;
        default:
            release_assert(idx > 3);
            break;
        }

        if (load_str != '\0')
        {
            char *inst = STRfmt("%cload_%c", type_str, load_str);
            inst0(inst);
            free(inst);
        }
        else
        {
            char *inst = STRfmt("%cload", type_str);
            inst1(inst, idx);
            free(inst);
        }
    }
    else if (level > 0)
    {
        ptrdiff_t idx = IDXdeep_lookup(index_table, VAR_NAME(node));

        char *inst = STRfmt("%cloadn", type_str);
        inst2(inst, level, idx);
        free(inst);
    }
    else // level < 0
    {
        if (NODE_TYPE(entry) == NT_GLOBALDEC)
        {
            char *inst = STRfmt("%cloade", type_str);
            inst1(inst, IDXlookup(import_table, VAR_NAME(node)));
            free(inst);
        }
        else
        {
            ptrdiff_t idx = IDXdeep_lookup(index_table, VAR_NAME(node));

            char *inst = STRfmt("%cloadg", type_str);
            inst1(inst, idx);
            free(inst);
        }
    }

    return node;
}

node_st *CG_CGarrayvar(node_st *node)
{
    // Only for type checking
    bool parent_is_expr = is_expr;
    is_expr = false;
    TRAVchildren(node);
    is_expr = parent_is_expr;
    return node;
}

node_st *CG_CGdimensionvars(node_st *node)
{
    // Only for type checking
    enum DataType parent_type = type;
    bool parent_is_expr = is_expr;
    type = DT_int;
    is_expr = false;
    TRAVchildren(node);
    type = parent_type;
    is_expr = parent_is_expr;
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
    if (is_expr == false)
    {
        return node;
    }

    // Type checking only
    int level = INT_MAX;
    node_st *entry = deep_lookup_level(current, VAR_NAME(ARRAYEXPR_VAR(node)), &level);
    enum DataType has_type = symbol_to_type(entry);
    release_assert(has_type != DT_NULL);
    release_assert(has_type != DT_void);
    release_assert(type == has_type);
    release_assert(level != INT_MAX);

    // Calculate index
    enum DataType parent_type = type;
    type = DT_int;
    TRAVopt(EXPRS_EXPR(ARRAYEXPR_DIMS(node)));
    type = parent_type;
    release_assert(EXPRS_NEXT(ARRAYEXPR_DIMS(node)) == NULL); // Only 1d allowed

    if (level == 0)
    {
        // Load the array reference
        ptrdiff_t idx = IDXdeep_lookup(index_table, VAR_NAME(ARRAYEXPR_VAR(node)));

        char load_str = '\0';
        switch (idx)
        {
        case 0:
            load_str = '0';
            break;
        case 1:
            load_str = '1';
            break;
        case 2:
            load_str = '2';
            break;
        case 3:
            load_str = '3';
            break;
        default:
            release_assert(idx > 3);
            break;
        }

        if (load_str != '\0')
        {
            char *inst = STRfmt("aload_%c", load_str);
            inst0(inst);
            free(inst);
        }
        else
        {
            inst1("aload", idx);
        }
    }
    else if (level > 0)
    {
        // Load the array reference
        ptrdiff_t idx = IDXdeep_lookup(index_table, VAR_NAME(ARRAYEXPR_VAR(node)));

        inst2("aloadn", level, idx);
    }
    else // level < 0
    {
        if (NODE_TYPE(entry) == NT_GLOBALDEC)
        {
            inst1("aloade", IDXlookup(import_table, VAR_NAME(ARRAYEXPR_VAR(node))));
        }
        else
        {
            // Load the array reference
            ptrdiff_t idx = IDXdeep_lookup(index_table, VAR_NAME(ARRAYEXPR_VAR(node)));

            inst1("aloadg", idx);
        }
    }

    // Load the array element
    switch (has_type)
    {
    case DT_NULL:
    case DT_void:
        release_assert(false);
        break;
    case DT_bool:
        inst0(is_arrayexpr_store ? "bstorea" : "bloada");
        break;
    case DT_int:
        inst0(is_arrayexpr_store ? "istorea" : "iloada");
        break;
    case DT_float:
        inst0(is_arrayexpr_store ? "fstorea" : "floada");
        break;
    }

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
    TRAVopt(ARRAYASSIGN_EXPR(node)); // calculate value to store to array

    bool parent_is_arrayexpr_store = is_arrayexpr_store;
    is_arrayexpr_store = true; // use store commands to array
    TRAVopt(ARRAYASSIGN_VAR(node));
    is_arrayexpr_store = parent_is_arrayexpr_store;

    type = parent_type;
    return node;
}

node_st *CG_CGint(node_st *node)
{
    release_assert(type == DT_int);
    int val = INT_VAL(node);
    if (is_expr == false)
    {
        char *val_str = int_to_str(val);
        if (HTlookup(constant_table, val_str) == NULL)
        {
            IDXinsert(constant_table, val_str, constant_counter++);
            consti(val);
        }
        else
        {
            free(val_str);
        }
        return node;
    }

    if (val == 0)
    {
        inst0("iloadc_0");
    }
    else if (val == 1)
    {
        inst0("iloadc_1");
    }
    else if (val == -1)
    {
        inst0("iloadc_m1");
    }
    else
    {
        char *val_str = int_to_str(val);
        if (HTlookup(constant_table, val_str) == NULL)
        {
            IDXinsert(constant_table, val_str, constant_counter++);
            consti(val);

            ptrdiff_t idx = IDXlookup(constant_table, val_str);
            inst1("iloadc", idx);
        }
        else
        {
            ptrdiff_t idx = IDXlookup(constant_table, val_str);
            inst1("iloadc", idx);

            free(val_str);
        }
    }

    return node;
}

node_st *CG_CGfloat(node_st *node)
{
    release_assert(type == DT_float);
    double val = FLOAT_VAL(node);
    if (val == 0.0)
    {
        inst0("floadc_0");
    }
    else if (val == 1.0)
    {
        inst0("floadc_1");
    }
    else
    {
        char *val_str = float_to_str(val);
        if (HTlookup(constant_table, val_str) == NULL)
        {
            IDXinsert(constant_table, val_str, constant_counter++);
            constf(val);

            ptrdiff_t idx = IDXlookup(constant_table, val_str);
            inst1("floadc", idx);
        }
        else
        {
            ptrdiff_t idx = IDXlookup(constant_table, val_str);
            inst1("floadc", idx);

            free(val_str);
        }
    }

    return node;
}

node_st *CG_CGbool(node_st *node)
{
    release_assert(type == DT_bool);
    if (BOOL_VAL(node) == true)
    {
        inst0("bloadc_t");
    }
    else
    {
        inst0("bloadc_f");
    }
    return node;
}

node_st *CG_CGternary(node_st *node)
{
    char *pfalse_label = STRfmt("pfalse%d", if_counter);
    char *end_label = STRfmt("pend%d", if_counter);
    if_counter++;

    enum DataType parent_type = type;
    type = DT_bool;
    TRAVopt(TERNARY_PRED(node));
    type = parent_type;
    instL("branch_f", pfalse_label);
    TRAVopt(TERNARY_PTRUE(node));
    instL("jump", end_label);

    label(pfalse_label);
    TRAVopt(TERNARY_PFALSE(node));
    label(end_label);

    free(pfalse_label);
    free(end_label);
    return node;
}
