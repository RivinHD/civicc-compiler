/**
 * @file
 *
 * This file contains the code for the Print traversal.
 * The traversal has the uid: PRT
 *
 *
 */

#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav.h"
#include "palm/dbug.h"
#include <stdio.h>

node_st *PRTprogram(node_st *node)
{
    TRAVdecls(node);
    return node;
}

node_st *PRTdeclarations(node_st *node)
{
    // TODO print something useful
    TRAVdecl(node);
    if (DECLARATIONS_NEXT(node) != NULL)
    {
        TRAVnext(node);
    }
    return node;
}

node_st *PRTfundec(node_st *node)
{
    if (FUNDEC_EXTERN_ATT(node))
    {
        printf("extern ");
    }
    TRAVfunHeader(node);
    return node;
}

node_st *PRTfundef(node_st *node)
{
    if (FUNDEF_EXPORT(node))
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
    if (GLOBALDEC_EXTERN_ATT(node))
    {
        printf("extern ");
    }
    TRAVtype(node);
    TRAVvar(node);
    printf(";");
    return node;
}

node_st *PRTglobaldef(node_st *node)
{
    if (GLOBALDEF_EXPORT(node))
    {
        printf("export ");
    }
    TRAVvarDec(node);
    printf(";");
    return node;
}

node_st *PRTfunheader(node_st *node)
{
    TRAVret(node);
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
    TRAVparam(node);
    if (PARAMS_NEXT(node) != NULL)
    {
        printf(", ");
        TRAVnext(node);
    }
    return node;
}

node_st *PRTparam(node_st *node)
{
    TRAVtype(node);
    printf(" ");
    TRAVvar(node);
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
    TRAVtype(node);
    printf(" ");
    TRAVvar(node);
    if (VARDEC_EXPR(node) != NULL)
    {
        printf(" ");
        TRAVexpr(node);
    }
    printf(";");
    return node;
}

node_st *PRTproccall(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTexprs(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTifstatement(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTelsestatement(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTwhileloop(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTdowhileloop(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTforloop(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTretstatement(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTcast(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTvoid(node_st *node)
{
    // TODO
    return node;
}

/**
 * @fn PRTvar
 */
node_st *PRTvar(node_st *node)
{
    printf("%s", VAR_NAME(node));
    return node;
}

node_st *PRTarrayvar(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTdimensionvars(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTarrayexpr(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTdimensionexprs(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTarrayinit(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTarrayassign(node_st *node)
{
    // TODO
    return node;
}
/**
 * @fn PRTnum
 */
node_st *PRTnum(node_st *node)
{
    printf("%d", NUM_VAL(node));
    return node;
}

/**
 * @fn PRTfloat
 */
node_st *PRTfloat(node_st *node)
{
    printf("%f", FLOAT_VAL(node));
    return node;
}

/**
 * @fn PRTbool
 */
node_st *PRTbool(node_st *node)
{
    char *bool_str = BOOL_VAL(node) ? "true" : "false";
    printf("%s", bool_str);
    return node;
}
