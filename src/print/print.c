/**
 * @file
 *
 * This file contains the code for the Print traversal.
 * The traversal has the uid: PRT
 *
 *
 */

#include "ccngen/ast.h"
#include "ccngen/trav.h"
#include "palm/dbug.h"

/**
 * @fn PRTprogram
 */
node_st *PRTprogram(node_st *node)
{
    // TODO
    // TRAVstatements(node);
    return node;
}

node_st *PRTdeclarations(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTfundec(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTfundef(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTglobaldec(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTglobaldef(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTfunheader(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTparams(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTparam(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTfunbody(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTvardecs(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTlocalfundefs(node_st *node)
{
    // TODO
    return node;
}

node_st *PRTlocalfundef(node_st *node)
{
    // TODO
    return node;
}

/**
 * @fn PRTstatements
 */
node_st *PRTstatements(node_st *node)
{
    // TODO
    // TRAVstatement(node);
    TRAVnext(node);
    return node;
}

/**
 * @fn PRTassign
 */
node_st *PRTassign(node_st *node)
{
    if (ASSIGN_VAR(node) != NULL)
    {
        TRAVlet(node);
        printf(" = ");
    }

    TRAVexpr(node);
    printf(";\n");

    return node;
}

/**
 * @fn PRTbinop
 */
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
    // TODO
    return node;
}

node_st *PRTvardec(node_st *node)
{
    // TODO
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
