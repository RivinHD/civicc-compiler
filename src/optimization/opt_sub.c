/**
 * @file
 *
 * This file contains the code for the OptSubstraction traversal.
 * The traversal has the uid: OS
 *
 *
 */

#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav.h"
#include "palm/str.h"

/**
 * @fn OSbinop
 */
node_st *OSbinop(node_st *node)
{
    TRAVleft(node);
    TRAVright(node);

    if (BINOP_OP(node) == BO_sub)
    {
        if ((NODE_TYPE(BINOP_LEFT(node)) == NT_VAR) && (NODE_TYPE(BINOP_RIGHT(node)) == NT_VAR) &&
            STReq(VAR_NAME(BINOP_LEFT(node)), VAR_NAME(BINOP_RIGHT(node))))
        {
            node = CCNfree(node);
            node = ASTint(0);
        }
        else if ((NODE_TYPE(BINOP_LEFT(node)) == NT_INT) && (NODE_TYPE(BINOP_RIGHT(node)) == NT_INT) &&
                 (INT_VAL(BINOP_LEFT(node)) == INT_VAL(BINOP_RIGHT(node))))
        {
            node = CCNfree(node);
            node = ASTint(0);
        }
    }

    return node;
}
