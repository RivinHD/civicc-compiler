#include "to_string.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/// Creates a string representation of a datatype. The returned char pointer needs to freed by the
/// caller.
char *datatype_to_string(enum DataType type)
{
    switch (type)
    {
    case DT_void:
        return STRcpy("void");
    case DT_int:
        return STRcpy("int");
    case DT_float:
        return STRcpy("float");
    case DT_bool:
        return STRcpy("bool");
    case DT_NULL:
    default:
        // unknown datatype detected
        release_assert(false);
        return NULL;
    }
}

/// Creates a string representation of a mono-op type. The returned char pointer needs to freed by
/// the caller.
char *monoptype_to_string(enum MonOpType type)
{
    switch (type)
    {
    case MO_not:
        return STRcpy("!");
    case MO_neg:
        return STRcpy("-");
    case MO_NULL:
    default:
        // unknown datatype detected
        release_assert(false);
        return NULL;
    }
}

/// Creates a string representation of a binary-op type. The returned char pointer needs to freed by
/// the caller.
char *binoptype_to_string(enum BinOpType type)
{
    switch (type)
    {
    case BO_add:
        return STRcpy("+");
    case BO_sub:
        return STRcpy("-");
    case BO_mul:
        return STRcpy("*");
    case BO_div:
        return STRcpy("/");
    case BO_mod:
        return STRcpy("%");
    case BO_lt:
        return STRcpy("<");
    case BO_le:
        return STRcpy("<=");
    case BO_gt:
        return STRcpy(">");
    case BO_ge:
        return STRcpy(">=");
    case BO_eq:
        return STRcpy("==");
    case BO_ne:
        return STRcpy("!=");
    case BO_and:
        return STRcpy("&&");
    case BO_or:
        return STRcpy("||");
    case BO_NULL:
    default:
        // unknown datatype detected
        release_assert(false);
        return NULL;
    }
}

bool has_child_next(node_st *node, node_st **child_next)
{
    release_assert(node != NULL);
    release_assert(child_next != NULL);

    switch (node->nodetype)
    {
    // nodes with next
    case NT_ARRAYINIT:
        *child_next = ARRAYINIT_NEXT(node);
        return true;
    case NT_DIMENSIONVARS:
        *child_next = DIMENSIONVARS_NEXT(node);
        return true;
    case NT_EXPRS:
        *child_next = EXPRS_NEXT(node);
        return true;
    case NT_STATEMENTS:
        *child_next = STATEMENTS_NEXT(node);
        return true;
    case NT_LOCALFUNDEFS:
        *child_next = LOCALFUNDEFS_NEXT(node);
        return true;
    case NT_VARDECS:
        *child_next = VARDECS_NEXT(node);
        return true;
    case NT_PARAMS:
        *child_next = PARAMS_NEXT(node);
        return true;
    case NT_DECLARATIONS:
        *child_next = DECLARATIONS_NEXT(node);
        return true;

    // nodes without next
    case NT_BOOL:
    case NT_FLOAT:
    case NT_INT:
    case NT_ARRAYASSIGN:
    case NT_ARRAYEXPR:
    case NT_ARRAYVAR:
    case NT_VAR:
    case NT_CAST:
    case NT_RETSTATEMENT:
    case NT_FORLOOP:
    case NT_DOWHILELOOP:
    case NT_WHILELOOP:
    case NT_IFSTATEMENT:
    case NT_PROCCALL:
    case NT_VARDEC:
    case NT_MONOP:
    case NT_BINOP:
    case NT_ASSIGN:
    case NT_FUNBODY:
    case NT_FUNHEADER:
    case NT_GLOBALDEF:
    case NT_GLOBALDEC:
    case NT_FUNDEF:
    case NT_FUNDEC:
    case NT_PROGRAM:
    case NT_TERNARY:
        return false;

    default:
    case _NT_SIZE:
    case NT_NULL:
        // unknown datatype detected
        release_assert(false);
        return false;
    }
}

char *_node_to_string_array(node_st *node, unsigned int depth, char *depth_string,
                            bool is_last_child)
{
    char *output = NULL;
    node_st *next = node;
    char *connection = "┢";

    while (next != NULL)
    {
        char *old_output = output;
        char *output_child = _node_to_string(next, depth + 1, connection, depth_string);
        connection = "┣";
        output = STRcat(old_output, output_child);
        free(output_child);
        free(old_output);
        node_st *next_next = NULL;
        release_assert(has_child_next(next, &next_next));
        release_assert(next_next != next);
        next = next_next;
    }

    // Last Element of the array is always NULL
    char *old_output = output;
    char *output_child =
        _node_to_string(NULL, depth + 1, (is_last_child ? "┗" : "┡"), depth_string);
    output = STRcat(old_output, output_child);
    free(output_child);
    free(old_output);

    return output;
}

/// Gets the string representation of a node without its children
char *get_node_name(node_st *node)
{
    char *node_string = NULL;
    if (node == NULL)
    {
        node_string = STRcpy("NULL");
    }
    else
    {

        char *extra_string = NULL; // can be used for additional argument parsing
        switch (node->nodetype)
        {
        case NT_BOOL:
            node_string = STRfmt("Bool -- val:'%d'", BOOL_VAL(node));
            break;
        case NT_FLOAT:
            node_string = STRfmt("Float -- val:'%f'", FLOAT_VAL(node));
            break;
        case NT_INT:
            node_string = STRfmt("Int -- val:'%d'", INT_VAL(node));
            break;
        case NT_ARRAYASSIGN:
            node_string = STRcpy("ArrayAssign");
            break;
        case NT_ARRAYINIT:
            node_string = STRcpy("ArrayInit");
            break;
        case NT_ARRAYEXPR:
            node_string = STRcpy("ArrayExpr");
            break;
        case NT_DIMENSIONVARS:
            node_string = STRcpy("DimensionVars");
            break;
        case NT_ARRAYVAR:
            node_string = STRcpy("ArrayVar");
            break;
        case NT_VAR:
            node_string = STRfmt("Var -- name:'%s'", VAR_NAME(node));
            break;
        case NT_CAST:
            extra_string = datatype_to_string(CAST_TYPE(node));
            node_string = STRfmt("Cast -- type:'%s'", extra_string);
            break;
        case NT_RETSTATEMENT:
            node_string = STRcpy("RetStatement");
            break;
        case NT_FORLOOP:
            node_string = STRcpy("ForLoop");
            break;
        case NT_DOWHILELOOP:
            node_string = STRcpy("DoWhileLoop");
            break;
        case NT_WHILELOOP:
            node_string = STRcpy("WhileLoop");
            break;
        case NT_IFSTATEMENT:
            node_string = STRcpy("IfStatement");
            break;
        case NT_EXPRS:
            node_string = STRcpy("Exprs");
            break;
        case NT_PROCCALL:
            node_string = STRcpy("ProcCall");
            break;
        case NT_VARDEC:
            extra_string = datatype_to_string(VARDEC_TYPE(node));
            node_string = STRfmt("VarDec -- type:'%s'", extra_string);
            break;
        case NT_MONOP:
            extra_string = monoptype_to_string(MONOP_OP(node));
            node_string = STRfmt("Monop -- op:'%s'", extra_string);
            break;
        case NT_BINOP:
            extra_string = binoptype_to_string(BINOP_OP(node));
            node_string = STRfmt("Binop -- op:'%s'", extra_string);
            break;
        case NT_ASSIGN:
            node_string = STRcpy("Assign");
            break;
        case NT_STATEMENTS:
            node_string = STRcpy("Statements");
            break;
        case NT_LOCALFUNDEFS:
            node_string = STRcpy("LocalFunDefs");
            break;
        case NT_VARDECS:
            node_string = STRcpy("VarDecs");
            break;
        case NT_FUNBODY:
            node_string = STRcpy("FunBody");
            break;
        case NT_PARAMS:
            extra_string = datatype_to_string(PARAMS_TYPE(node));
            node_string = STRfmt("Params -- type:'%s'", extra_string);
            break;
        case NT_FUNHEADER:
            extra_string = datatype_to_string(FUNHEADER_TYPE(node));
            node_string = STRfmt("FunHeader -- type:'%s'", extra_string);
            break;
        case NT_GLOBALDEF:
            node_string = STRfmt("GlobalDef -- has_export:'%d'", GLOBALDEF_HAS_EXPORT(node));
            break;
        case NT_GLOBALDEC:
            extra_string = datatype_to_string(GLOBALDEC_TYPE(node));
            node_string = STRfmt("GlobalDec -- type:'%s'", extra_string);
            break;
        case NT_FUNDEF:
            node_string = STRfmt("FunDef -- has_export:'%d'", FUNDEF_HAS_EXPORT(node));
            break;
        case NT_FUNDEC:
            node_string = STRcpy("FunDec");
            break;
        case NT_DECLARATIONS:
            node_string = STRcpy("Declarations");
            break;
        case NT_PROGRAM:
            node_string = STRcpy("Program");
            break;
        case NT_TERNARY:
            node_string = STRcpy("Ternary");
            break;
        case NT_NULL:
        case _NT_SIZE:
        default:
            // unknown datatype detected
            release_assert(false);
            break;
        }

        if (extra_string != NULL)
        {
            free(extra_string);
        }
    }

    return node_string;
}

char *_node_to_string(node_st *node, unsigned int depth, const char *connection, char *depth_string)
{

    char *node_string = get_node_name(node);
    char *output = NULL;

    if (depth == 0)
    {
        output = STRfmt("%s\n", node_string);
    }
    else
    {
        // Amputate the depth string to size - 5 for the format and afterward place it back
        size_t len = STRlen(depth_string);
        size_t unit_length = (len >= 3 && depth_string[len - 3] == ' ' ? 3 : 5);
        size_t position = (len >= unit_length ? len - unit_length : 0);
        release_assert(position <= INT_MAX);
        char *sub = STRsubStr(depth_string, 0, (int)position);
        output = STRfmt("%s%s─ %s\n", sub, connection, node_string);
        free(sub);
    }

    if (node != NULL && node->num_children > 0)
    {
        node_st *child_next = NULL;

        for (unsigned int i = 0; i < node->num_children; i++)
        {
            node_st *child = node->children[i];
            if (child != NULL && has_child_next(child, &child_next))
            {
                break;
            }
        }

        bool child_next_is_last = has_child_next(node, &child_next) &&
                                  child_next == node->children[node->num_children - 1];
        // The last child is a special case
        for (unsigned int i = 0; i < node->num_children; i++)
        {
            node_st *child = node->children[i];
            char *output_child = NULL;

            // The array was already printed by the first node of the array of the same type
            if (has_child_next(node, &child_next) && child == child_next &&
                (child == NULL ? true : node->nodetype == child->nodetype) && depth != 0)
            {
                continue;
            }
            // Does the child start an array
            else if (child != NULL && has_child_next(child, &child_next))
            {
                char *next_depth_string = STRcat(depth_string, "┃  ");
                output_child =
                    _node_to_string_array(child, depth + 1, next_depth_string,
                                          i == node->num_children - 1 || child_next_is_last);
                free(next_depth_string);
            }
            else
            {
                // Standard node
                const char *connection =
                    (i == node->num_children - (child_next_is_last && depth != 0 ? 2 : 1)) ? "└"
                                                                                           : "├";
                const char *depth_ext =
                    (i == node->num_children - (child_next_is_last && depth != 0 ? 2 : 1)) ? "   "
                                                                                           : "│  ";
                char *next_depth_string = STRcat(depth_string, depth_ext);
                output_child = _node_to_string(child, depth + 1, connection, next_depth_string);
                free(next_depth_string);
            }

            char *output_old = output;
            output = STRcat(output_old, output_child);
            free(output_child);
            free(output_old);
        }
    }

    release_assert(node_string != NULL);
    free(node_string);

    release_assert(output != NULL);
    return output;
}

/// Creates a string representation of a node. The returned char pointer needs to freed by the
/// caller.
char *node_to_string(node_st *node)
{
    return _node_to_string(node, 0, "", "");
}

char *_funheader_params_to_oneliner_string(node_st *node)
{
    if (NODE_TYPE(node) != NT_FUNHEADER)
    {
        return NULL;
    }

    char *output = NULL;
    node_st *param = FUNHEADER_PARAMS(node);
    if (param == NULL)
    {
        return NULL;
    }

    while (PARAMS_NEXT(param) != NULL)
    {

        char *type = datatype_to_string(PARAMS_TYPE(param));
        char *node_name = get_node_name(PARAMS_VAR(param));
        char *text = STRfmt("%s (%s), ", type, node_name);
        char *output_old = output;
        output = STRcat(output_old, text);
        free(text);
        free(output_old);
        free(node_name);
        free(type);

        param = PARAMS_NEXT(param);
    }

    release_assert(param != NULL);
    char *type = datatype_to_string(PARAMS_TYPE(param));
    char *node_name = get_node_name(PARAMS_VAR(param));
    char *text = STRfmt("%s (%s)", type, node_name);
    char *output_old = output;
    output = STRcat(output_old, text);
    free(text);
    free(output_old);
    free(node_name);
    free(type);

    return output;
}

char *_symbols_to_string(node_st *node, htable_stptr htable, uint32_t counter)
{
    if (node == NULL)
    {
        return NULL;
    }

    char *output = NULL;
    htable_stptr symbols = NULL;
    if (NODE_TYPE(node) == NT_PROGRAM)
    {
        symbols = PROGRAM_SYMBOLS(node);
    }
    else if (NODE_TYPE(node) == NT_FUNDEF)
    {
        symbols = FUNDEF_SYMBOLS(node);
    }

    if (symbols != NULL)
    {

        void *parent = NULL;
        for (htable_iter_st *iter = HTiterate(symbols); iter; iter = HTiterateNext(iter))
        {
            // Getter functions to extract htable elements
            char *key = HTiterKey(iter);
            node_st *value = HTiterValue(iter);

            if (STReq(key, htable_parent_name))
            {
                release_assert(parent == NULL);
                parent = value;
            }
            else
            {
                char *text = NULL;
                char *node_name = NULL;
                if (NODE_TYPE(value) == NT_FUNDEF || NODE_TYPE(value) == NT_FUNDEC)
                {
                    node_st *funheader = NODE_TYPE(value) == NT_FUNDEF ? FUNDEF_FUNHEADER(value)
                                                                       : FUNDEC_FUNHEADER(value);
                    node_name = get_node_name(funheader);
                    char *params = _funheader_params_to_oneliner_string(funheader);
                    text = STRfmt("├─ %s: %s -- Params: %s\n", key, node_name, params);
                    free(params);
                }
                else
                {
                    node_name = get_node_name(value);
                    text = STRfmt("├─ %s: %s\n", key, node_name);
                }
                free(node_name);

                char *output_old = output;
                output = STRcat(output_old, text);
                free(output_old);
                free(text);
            }
        }

        char *output_old = output;
        char *node_name = NULL;
        if (NODE_TYPE(node) == NT_FUNDEF)
        {
            node_name = STRfmt("FunDef '%s'", VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(node))));
        }
        else
        {
            node_name = get_node_name(node);
        }

        if (output_old == NULL)
        {
            output_old = STRcpy("");
        }

        HTinsert(htable, symbols, STRfmt("%d: %s", counter, node_name));
        if (parent != NULL)
        {
            const char *parent_name = HTlookup(htable, parent);
            release_assert(parent_name != NULL);
            output = STRfmt("┌─ %d: %s -- parent: '%s'\n%s└────────────────────\n\n", counter,
                            node_name, parent_name, output_old);
        }
        else
        {
            output =
                STRfmt("┌─ %d: %s\n%s└────────────────────\n\n", counter, node_name, output_old);
        }

        free(output_old);
        free(node_name);
        counter++;
    }

    for (unsigned int i = 0; i < node->num_children; i++)
    {
        char *output_child = _symbols_to_string(node->children[i], htable, counter);
        char *output_old = output;
        output = STRcat(output_old, output_child);
        free(output_child);
        free(output_old);
    }

    return output;
}
/// Creates a string representation of all symbol tables. The returned char pointer needs to freed
/// by the caller.
char *symbols_to_string(node_st *node)
{
    htable_stptr htable = HTnew_Ptr(1 << 6);
    char *output = _symbols_to_string(node, htable, 0);
    for (htable_iter_st *iter = HTiterate(htable); iter; iter = HTiterateNext(iter))
    {
        void *value = HTiterValue(iter);
        free(value);
    }
    HTdelete(htable);
    return output;
}

/// Creates a string representtation of the idx tables. The returned char pointer needs to freed
/// by the caller.
char *idx_to_string(htable_stptr idxtable)
{
    char *output = NULL;

    if (idxtable == NULL)
    {
        return output;
    }

    htable_stptr parent = NULL;
    for (htable_iter_st *iter = HTiterate(idxtable); iter; iter = HTiterateNext(iter))
    {
        // Getter functions to extract htable elements
        char *key = HTiterKey(iter);
        void *entry = HTiterValue(iter);
        release_assert(key != NULL);
        release_assert(entry != NULL);

        if (STReq(key, htable_parent_name))
        {
            release_assert(parent == NULL);
            parent = entry;
        }
        else
        {
            ptrdiff_t value = (ptrdiff_t)entry - 1;
            char *text = STRfmt("├─ %s: %d\n", key, value);

            char *output_old = output;
            output = STRcat(output_old, text);
            free(output_old);
            free(text);
        }
    }

    char *output_old = output;
    if (output_old == NULL)
    {
        output_old = STRcpy("");
    }

    if (parent != NULL)
    {
        char *parent_output = idx_to_string(parent);
        output = STRfmt("┌─ %p -- parent: '%p'\n%s└────────────────────\n\n %s", idxtable, parent,
                        output_old, parent_output);
        free(parent_output);
    }
    else
    {
        output = STRfmt("┌─ %p\n%s└────────────────────\n\n", idxtable, output_old);
    }

    free(output_old);

    return output;
}
