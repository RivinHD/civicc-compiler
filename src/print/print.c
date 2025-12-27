/**
 * This file contains the code for the Print traversal.
 * The traversal has the uid: PRT
 */

#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav.h"
#include "palm/dbug.h"
#include <stdbool.h>
#include <stdio.h>

void printDataType(enum DataType type)
{
    switch (type)
    {
    case DT_void:
        printf("void");
        break;
    case DT_int:
        printf("int");
        break;
    case DT_float:
        printf("float");
        break;
    case DT_bool:
        printf("bool");
        break;
    case DT_NULL:
        DBUG_ASSERT(false, "unknown data type detected!");
    }
}

node_st *PRTprogram(node_st *node)
{
    TRAVdecls(node);
    return node;
}

node_st *PRTdeclarations(node_st *node)
{

    TRAVdecl(node);
    if (DECLARATIONS_NEXT(node) != NULL)
    {
        printf("\n");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTfundec(node_st *node)
{
    printf("extern ");
    TRAVfunHeader(node);
    return node;
}

node_st *PRTfundef(node_st *node)
{
    if (FUNDEF_HAS_EXPORT(node))
    {
        printf("export ");
    }
    TRAVfunHeader(node);
    printf("{\n");
    TRAVfunBody(node);
    printf("}\n");
    return node;
}

node_st *PRTglobaldec(node_st *node)
{
    printf("extern ");
    printDataType(GLOBALDEC_TYPE(node));
    printf(" ");
    TRAVvar(node);
    printf(";");
    return node;
}

node_st *PRTglobaldef(node_st *node)
{
    if (GLOBALDEF_HAS_EXPORT(node))
    {
        printf("export ");
    }
    TRAVvarDec(node);
    printf(";");
    return node;
}

node_st *PRTfunheader(node_st *node)
{
    printDataType(FUNHEADER_TYPE(node));
    printf(" ");
    TRAVvar(node);
    printf("(");
    if (FUNHEADER_PARAMS(node) != NULL)
    {
        TRAVparams(node);
    }
    printf(")");
    return node;
}

node_st *PRTparams(node_st *node)
{
    printDataType(PARAMS_TYPE(node));
    printf(" ");
    TRAVvar(node);
    if (PARAMS_NEXT(node) != NULL)
    {
        printf(", ");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTfunbody(node_st *node)
{
    if (FUNBODY_VARDECS(node) != NULL)
    {
        printf("\n");
        TRAVvarDecs(node);
    }
    if (FUNBODY_LOCALFUNDEFS(node) != NULL)
    {
        printf("\n");
        TRAVlocalFunDefs(node);
    }
    if (FUNBODY_STMTS(node) != NULL)
    {
        printf("\n");
        TRAVstmts(node);
    }
    return node;
}

node_st *PRTvardecs(node_st *node)
{
    TRAVvarDec(node);
    if (VARDECS_NEXT(node) != NULL)
    {
        printf("\n");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTlocalfundefs(node_st *node)
{
    TRAVlocalFunDef(node);
    if (LOCALFUNDEFS_NEXT(node) != NULL)
    {
        printf("\n");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTlocalfundef(node_st *node)
{
    TRAVfunHeader(node);
    printf("\n");
    TRAVfunBody(node);
    return node;
}

node_st *PRTstatements(node_st *node)
{
    TRAVstmt(node);
    if (STATEMENTS_NEXT(node) != NULL)
    {
        printf("\n");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTassign(node_st *node)
{
    if (ASSIGN_VAR(node) != NULL)
    {
        TRAVvar(node);
        printf(" = ");
    }

    TRAVexpr(node);
    printf(";\n");

    return node;
}

node_st *PRTbinop(node_st *node)
{
    char *tmp = NULL;
    printf("( ");

    TRAVleft(node);

    switch (BINOP_OP(node))
    {
    case BO_add:
        tmp = "+";
        break;
    case BO_sub:
        tmp = "-";
        break;
    case BO_mul:
        tmp = "*";
        break;
    case BO_div:
        tmp = "/";
        break;
    case BO_mod:
        tmp = "%";
        break;
    case BO_lt:
        tmp = "<";
        break;
    case BO_le:
        tmp = "<=";
        break;
    case BO_gt:
        tmp = ">";
        break;
    case BO_ge:
        tmp = ">=";
        break;
    case BO_eq:
        tmp = "==";
        break;
    case BO_ne:
        tmp = "!=";
        break;
    case BO_or:
        tmp = "||";
        break;
    case BO_and:
        tmp = "&&";
        break;
    case BO_NULL:
        DBUG_ASSERT(false, "unknown binop detected!");
    }

    printf(" %s ", tmp);

    TRAVright(node);

    printf(")(%d:%d-%d)", NODE_BLINE(node), NODE_BCOL(node), NODE_ECOL(node));

    return node;
}

node_st *PRTmonop(node_st *node)
{
    char *op = "";
    switch (MONOP_OP(node))
    {
    case MO_neg:
        op = "-";
        break;
    case MO_not:
        op = "!";
        break;
    case MO_NULL:
        DBUG_ASSERT(false, "unkown monop detected!");
    }

    printf("( %s ", op);
    TRAVleft(node);
    printf(")");
    return node;
}

node_st *PRTvardec(node_st *node)
{
    printDataType(VARDEC_TYPE(node));
    printf(" ");
    TRAVvar(node);
    bool is_array = NODE_TYPE(VARDEC_VAR(node)) == NT_ARRAYEXPR;
    if (VARDEC_EXPR(node) != NULL)
    {
        printf(" = ");
        if (is_array)
        {
            printf("[");
            TRAVexpr(node);
            printf("]");
        }
        else
        {
            TRAVexpr(node);
        }
    }
    return node;
}

node_st *PRTproccall(node_st *node)
{
    TRAVvar(node);
    printf("(");
    if (PROCCALL_EXPRS(node) != NULL)
    {
        TRAVexprs(node);
    }
    printf(")");
    return node;
}

node_st *PRTexprs(node_st *node)
{
    TRAVexpr(node);
    if (EXPRS_NEXT(node) != NULL)
    {
        printf(", ");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTifstatement(node_st *node)
{
    printf("if (");
    TRAVexpr(node);
    printf(") {\n");
    TRAVblock(node);
    printf("\n}");
    if (IFSTATEMENT_ELSE_BLOCK(node) != NULL)
    {
        printf(" else ");
        TRAVelse_block(node);
    }
    return node;
}

node_st *PRTelsestatement(node_st *node)
{
    printf("{\n");
    TRAVblock(node);
    printf("\n}");
    return node;
}

node_st *PRTwhileloop(node_st *node)
{
    printf("while (");
    TRAVexpr(node);
    printf(") {\n");
    TRAVblock(node);
    printf("\n}");
    return node;
}

node_st *PRTdowhileloop(node_st *node)
{
    printf("do {\n");
    TRAVblock(node);
    printf("\n} while (");
    TRAVexpr(node);
    printf(")");
    return node;
}

node_st *PRTforloop(node_st *node)
{
    printf("for (");
    TRAVassign(node);
    printf(", ");
    TRAVcond(node);
    if (FORLOOP_ITER(node) != NULL)
    {
        printf(", ");
        TRAViter(node);
    }
    printf(") {\n");
    TRAVblock(node);
    printf("\n}");
    return node;
}

node_st *PRTretstatement(node_st *node)
{
    printf("return");
    if (RETSTATEMENT_EXPR(node) != NULL)
    {
        printf(" ");
        TRAVexpr(node);
    }
    printf(";");
    return node;
}

node_st *PRTcast(node_st *node)
{
    printf("(");
    printDataType(CAST_TYPE(node));
    printf(")");
    TRAVexpr(node);
    return node;
}

node_st *PRTvoid(node_st *node)
{
    printf("void");
    return node;
}

node_st *PRTvar(node_st *node)
{
    printf("%s", VAR_NAME(node));
    return node;
}

node_st *PRTarrayvar(node_st *node)
{
    printf("[");
    TRAVdims(node);
    printf("] ");
    TRAVvar(node);
    return node;
}

node_st *PRTdimensionvars(node_st *node)
{
    TRAVdim(node);
    if (DIMENSIONVARS_NEXT(node) != NULL)
    {
        printf(", ");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTarrayexpr(node_st *node)
{
    TRAVvar(node);
    printf("[");
    TRAVdims(node);
    printf("]");
    return node;
}

node_st *PRTarrayinit(node_st *node)
{
    TRAVexpr(node);
    if (ARRAYINIT_NEXT(node) != NULL)
    {
        printf(", ");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTarrayassign(node_st *node)
{
    TRAVvar(node);
    printf(" = ");
    TRAVexpr(node);
    return node;
}

node_st *PRTint(node_st *node)
{
    printf("%d", INT_VAL(node));
    return node;
}

node_st *PRTfloat(node_st *node)
{
    printf("%f", FLOAT_VAL(node));
    return node;
}

node_st *PRTbool(node_st *node)
{
    char *bool_str = BOOL_VAL(node) ? "true" : "false";
    printf("%s", bool_str);
    return node;
}
