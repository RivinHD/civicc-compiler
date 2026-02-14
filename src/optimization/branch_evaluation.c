#include "release_assert.h"
#include <ccn/dynamic_core.h>
#include <ccngen/ast.h>
#include <ccngen/enum.h>

static node_st *extracted_stmts_first = NULL;
static node_st *extracted_stmts_last = NULL;

void reset()
{
    extracted_stmts_first = NULL;
    extracted_stmts_last = NULL;
}

node_st *OPT_BEprogram(node_st *node)
{
    reset();
    TRAVchildren(node);
    return node;
}

node_st *OPT_BEstatements(node_st *node)
{
    node_st *parent_extraced_stmts_first = extracted_stmts_first;
    node_st *parent_extraced_stmts_last = extracted_stmts_last;
    extracted_stmts_first = NULL;
    extracted_stmts_last = NULL;
    TRAVchildren(node);
    if (extracted_stmts_first != NULL)
    {
        release_assert(extracted_stmts_last != NULL);
        release_assert(STATEMENTS_NEXT(extracted_stmts_last) == NULL);
        STATEMENTS_NEXT(extracted_stmts_last) = STATEMENTS_NEXT(node);
        STATEMENTS_NEXT(node) = extracted_stmts_first;
    }
    extracted_stmts_first = parent_extraced_stmts_first;
    extracted_stmts_last = parent_extraced_stmts_last;
    return node;
}

node_st *OPT_BEifstatement(node_st *node)
{
    TRAVchildren(node);
    if (NODE_TYPE(IFSTATEMENT_EXPR(node)) == NT_BOOL)
    {
        // Remove the if and else block so it becomes dead code.
        if (BOOL_VAL(IFSTATEMENT_EXPR(node)))
        {
            extracted_stmts_first = IFSTATEMENT_BLOCK(node);
            node_st *stmts = IFSTATEMENT_BLOCK(node);
            while (STATEMENTS_NEXT(stmts) != NULL)
            {
                stmts = STATEMENTS_NEXT(stmts);
            }
            extracted_stmts_last = stmts;
            CCNfree(IFSTATEMENT_ELSE_BLOCK(node));
        }
        else
        {
            extracted_stmts_first = IFSTATEMENT_ELSE_BLOCK(node);
            node_st *stmts = IFSTATEMENT_ELSE_BLOCK(node);
            while (STATEMENTS_NEXT(stmts) != NULL)
            {
                stmts = STATEMENTS_NEXT(stmts);
            }
            extracted_stmts_last = stmts;
            CCNfree(IFSTATEMENT_BLOCK(node));
        }

        IFSTATEMENT_BLOCK(node) = NULL;
        IFSTATEMENT_ELSE_BLOCK(node) = NULL;
    }
    return node;
}

node_st *OPT_BEternary(node_st *node)
{
    TRAVchildren(node);
    if (NODE_TYPE(TERNARY_PRED(node)) == NT_BOOL)
    {
        node_st *expr = NULL;
        if (BOOL_VAL(TERNARY_PRED(node)))
        {
            expr = TERNARY_PTRUE(node);
            TERNARY_PTRUE(node) = NULL;
        }
        else
        {
            expr = TERNARY_PFALSE(node);
            TERNARY_PFALSE(node) = NULL;
        }
        CCNfree(node);
        return expr;
    }
    return node;
}

node_st *OPT_BEwhileloop(node_st *node)
{
    TRAVchildren(node);
    if (NODE_TYPE(WHILELOOP_EXPR(node)) == NT_BOOL && !BOOL_VAL(WHILELOOP_EXPR(node)))
    {
        // Remove the block so it becomes dead code
        CCNfree(WHILELOOP_BLOCK(node));
        WHILELOOP_BLOCK(node) = NULL;
    }
    return node;
}

node_st *OPT_BEforloop(node_st *node)
{
    TRAVchildren(node);
    if (NODE_TYPE(FORLOOP_COND(node)) == NT_INT &&
        NODE_TYPE(ASSIGN_EXPR(FORLOOP_ASSIGN(node))) == NT_INT &&
        (FORLOOP_ITER(node) == NULL || NODE_TYPE(FORLOOP_ITER(node)) == NT_INT))
    {
        long start = INT_VAL(ASSIGN_EXPR(FORLOOP_ASSIGN(node)));
        long end = INT_VAL(FORLOOP_COND(node));
        long step = FORLOOP_ITER(node) == NULL ? 1 : INT_VAL(FORLOOP_ITER(node));

        if ((step > 0 && (start + step >= end)) || (step < 0 && (start + step <= end)))
        {
            // For loop is never executed. Remove the block so it becomes dead code
            CCNfree(FORLOOP_BLOCK(node));
            FORLOOP_BLOCK(node) = NULL;
        }
    }
    return node;
}

node_st *OPT_BEdowhileloop(node_st *node)
{
    TRAVchildren(node);
    if (NODE_TYPE(DOWHILELOOP_EXPR(node)) == NT_BOOL && !BOOL_VAL(DOWHILELOOP_EXPR(node)))
    {
        // Extract the block, so it becomes dead code.
        extracted_stmts_first = DOWHILELOOP_BLOCK(node);
        node_st *stmts = DOWHILELOOP_BLOCK(node);
        while (STATEMENTS_NEXT(stmts) != NULL)
        {
            stmts = STATEMENTS_NEXT(stmts);
        }
        extracted_stmts_last = stmts;
        DOWHILELOOP_BLOCK(node) = NULL;
    }
    return node;
}
