#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "palm/str.h"
#include "release_assert.h"
#include <stdlib.h>

/// Creates a string representation of a datatype. The returned char pointer needs to freed by the
/// caller.
char *datatype_to_string(enum DataType type)
{
    switch (type)
    {
    case DT_void:
        return STRfmt("void");
    case DT_int:
        return STRfmt("int");
    case DT_float:
        return STRfmt("float");
    case DT_bool:
        return STRfmt("bool");
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
        return STRfmt("!");
    case MO_neg:
        return STRfmt("-");
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
        return STRfmt("+");
    case BO_sub:
        return STRfmt("-");
    case BO_mul:
        return STRfmt("*");
    case BO_div:
        return STRfmt("/");
    case BO_mod:
        return STRfmt("%");
    case BO_lt:
        return STRfmt("<");
    case BO_le:
        return STRfmt("<=");
    case BO_gt:
        return STRfmt(">");
    case BO_ge:
        return STRfmt(">=");
    case BO_eq:
        return STRfmt("==");
    case BO_ne:
        return STRfmt("!=");
    case BO_and:
        return STRfmt("&&");
    case BO_or:
        return STRfmt("||");
    case BO_NULL:
    default:
        // unknown datatype detected
        release_assert(false);
        return NULL;
    }
}

char *_node_to_string(node_st *node, unsigned int depth, const char *connection, char *depth_string)
{
    char *node_string = NULL;
    if (node == NULL)
    {
        node_string = STRfmt("NULL");
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
            node_string = STRfmt("Int -- val:'%d'", FLOAT_VAL(node));
            break;
        case NT_ARRAYASSIGN:
            node_string = STRfmt("ArrayAssign");
            break;
        case NT_ARRAYINIT:
            node_string = STRfmt("ArrayInit");
            break;
        case NT_ARRAYEXPR:
            node_string = STRfmt("ArrayExpr");
            break;
        case NT_DIMENSIONVARS:
            node_string = STRfmt("DimensionVars");
            break;
        case NT_ARRAYVAR:
            node_string = STRfmt("ArrayVar");
            break;
        case NT_VAR:
            node_string = STRfmt("Var -- name:'%s'", VAR_NAME(node));
            break;
        case NT_CAST:
            extra_string = datatype_to_string(CAST_TYPE(node));
            node_string = STRfmt("Cast -- type:'%s'", extra_string);
            break;
        case NT_RETSTATEMENT:
            node_string = STRfmt("RetStatement");
            break;
        case NT_FORLOOP:
            node_string = STRfmt("ForLoop");
            break;
        case NT_DOWHILELOOP:
            node_string = STRfmt("DoWhileLoop");
            break;
        case NT_WHILELOOP:
            node_string = STRfmt("WhileLoop");
            break;
        case NT_IFSTATEMENT:
            node_string = STRfmt("IfStatement");
            break;
        case NT_EXPRS:
            node_string = STRfmt("Exprs");
            break;
        case NT_PROCCALL:
            node_string = STRfmt("ProcCall");
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
            node_string = STRfmt("Assign");
            break;
        case NT_STATEMENTS:
            node_string = STRfmt("Statements");
            break;
        case NT_LOCALFUNDEFS:
            node_string = STRfmt("LocalFunDefs");
            break;
        case NT_VARDECS:
            node_string = STRfmt("VarDecs");
            break;
        case NT_FUNBODY:
            node_string = STRfmt("FunBody");
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
            node_string = STRfmt("FunDec");
            break;
        case NT_DECLARATIONS:
            node_string = STRfmt("Declarations");
            break;
        case NT_PROGRAM:
            node_string = STRfmt("Program");
            break;
        case NT_NULL:
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

    char *output = NULL;

    if (depth == 0)
    {
        output = STRfmt("%s\n", node_string);
    }
    else
    {
        // Amputate the depth string to size - 5 for the format and afterward place it back
        unsigned int position = STRlen(depth_string) - 5;
        char depth_replace = depth_string[position];
        depth_string[position] = '\0';
        output = STRfmt("%s%s─ %s\n", depth_string, connection, node_string);
        depth_string[position] = depth_replace;
    }

    if (node != NULL)
    {
        // The last child is a special case
        for (unsigned int i = 0; i < node->num_children - 1; i++)
        {
            char *next_depth_string = STRcat(depth_string, "│  ");
            char *output_child =
                _node_to_string(node->children[i], depth + 1, "├", next_depth_string);
            char *output_old = output;
            output = STRcat(output_old, output_child);
            free(next_depth_string);
            free(output_child);
            free(output_old);
        }

        // Handel the last children
        if (node->num_children > 0)
        {
            char *next_depth_string = STRcat(depth_string, "ﾠ  ");
            node_st *child_last = node->children[node->num_children - 1];
            char *output_child = _node_to_string(child_last, depth + 1, "└", next_depth_string);
            char *output_old = output;
            output = STRcat(output_old, output_child);
            free(next_depth_string);
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
