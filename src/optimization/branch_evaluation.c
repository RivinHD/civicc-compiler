#include "palm/hash_table.h"
#include "release_assert.h"
#include "user_types.h"
#include <ccn/dynamic_core.h>
#include <ccn/phase_driver.h>
#include <ccngen/ast.h>
#include <ccngen/enum.h>
#include <stdio.h>

static node_st *extracted_stmts_first = NULL;
static node_st *extracted_stmts_last = NULL;
static htable_stptr assign_table = NULL;
static bool conditioned_assign = false;

void reset()
{
    extracted_stmts_first = NULL;
    extracted_stmts_last = NULL;
    assign_table = NULL;
    conditioned_assign = false;
}

node_st *OPT_BEprogram(node_st *node)
{
    reset();
    assign_table = HTnew_String(2 << 4);
    TRAVchildren(node);
    HTdelete(assign_table);
    return node;
}

node_st *OPT_BEfundef(node_st *node)
{
    htable_stptr parent_temp_table = assign_table;
    assign_table = HTnew_String(2 << 4);
    TRAVchildren(node);
    HTdelete(assign_table);
    assign_table = parent_temp_table;
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
        CCNcycleNotify();
    }
    extracted_stmts_first = parent_extraced_stmts_first;
    extracted_stmts_last = parent_extraced_stmts_last;
    return node;
}

node_st *OPT_BEassign(node_st *node)
{
    TRAVchildren(node);

    char *name = VAR_NAME(ASSIGN_VAR(node));

    // We need to remove any assign it was condition because we do not know the actual assigned
    // value
    if (conditioned_assign)
    {
        HTremove(assign_table, name);
    }
    else
    {
        bool success = HTinsert(assign_table, name, ASSIGN_EXPR(node));
        release_assert(success);
    }
    return node;
}

node_st *OPT_BEifstatement(node_st *node)
{
    node_st *expr = IFSTATEMENT_EXPR(node);

    // We can extract values from temp variables to get possible further optimization
    while (NODE_TYPE(expr) == NT_VAR)
    {
        node_st *entry = HTlookup(assign_table, VAR_NAME(expr));
        if (entry == NULL)
        {
            break;
        }
        expr = entry;
    }

    if (NODE_TYPE(expr) == NT_BOOL)
    {
        // Remove the if and else block so it becomes dead code.
        if (BOOL_VAL(expr))
        {
            extracted_stmts_first = IFSTATEMENT_BLOCK(node);
            node_st *stmts = IFSTATEMENT_BLOCK(node);
            if (stmts != NULL)
            {
                while (STATEMENTS_NEXT(stmts) != NULL)
                {
                    stmts = STATEMENTS_NEXT(stmts);
                }
            }
            extracted_stmts_last = stmts;
            CCNfree(IFSTATEMENT_ELSE_BLOCK(node));
            CCNcycleNotify();
        }
        else
        {
            extracted_stmts_first = IFSTATEMENT_ELSE_BLOCK(node);
            node_st *stmts = IFSTATEMENT_ELSE_BLOCK(node);
            if (stmts != NULL)
            {
                while (STATEMENTS_NEXT(stmts) != NULL)
                {
                    stmts = STATEMENTS_NEXT(stmts);
                }
            }
            extracted_stmts_last = stmts;
            CCNfree(IFSTATEMENT_BLOCK(node));
            CCNcycleNotify();
        }

        IFSTATEMENT_BLOCK(node) = NULL;
        IFSTATEMENT_ELSE_BLOCK(node) = NULL;
    }

    conditioned_assign = true;
    TRAVchildren(node);
    conditioned_assign = false;

    return node;
}

node_st *OPT_BEternary(node_st *node)
{
    node_st *pred = TERNARY_PRED(node);
    while (NODE_TYPE(pred) == NT_VAR)
    {
        node_st *entry = HTlookup(assign_table, VAR_NAME(pred));
        if (entry == NULL)
        {
            break;
        }
        pred = entry;
    }

    if (NODE_TYPE(pred) == NT_BOOL)
    {
        node_st *expr = NULL;
        if (BOOL_VAL(pred))
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
        CCNcycleNotify();
        TRAVchildren(expr);
        return expr;
    }

    TRAVchildren(node);
    return node;
}

node_st *OPT_BEwhileloop(node_st *node)
{
    node_st *expr = WHILELOOP_EXPR(node);
    while (NODE_TYPE(expr) == NT_VAR)
    {
        node_st *entry = HTlookup(assign_table, VAR_NAME(expr));
        if (entry == NULL)
        {
            break;
        }
        expr = entry;
    }

    if (NODE_TYPE(expr) == NT_BOOL && !BOOL_VAL(expr))
    {
        // Remove the block so it becomes dead code
        CCNfree(WHILELOOP_BLOCK(node));
        WHILELOOP_BLOCK(node) = NULL;
        CCNcycleNotify();
    }

    conditioned_assign = true;
    TRAVchildren(node);
    conditioned_assign = false;

    return node;
}

node_st *OPT_BEforloop(node_st *node)
{
    node_st *expr = ASSIGN_EXPR(FORLOOP_ASSIGN(node));
    node_st *cond = FORLOOP_COND(node);
    node_st *iter = FORLOOP_ITER(node);
    while (NODE_TYPE(expr) == NT_VAR)
    {
        node_st *entry = HTlookup(assign_table, VAR_NAME(expr));
        if (entry == NULL)
        {
            break;
        }
        expr = entry;
    }
    while (NODE_TYPE(cond) == NT_VAR)
    {
        node_st *entry = HTlookup(assign_table, VAR_NAME(cond));
        if (entry == NULL)
        {
            break;
        }
        cond = entry;
    }
    while (iter != NULL && NODE_TYPE(iter) == NT_VAR)
    {
        node_st *entry = HTlookup(assign_table, VAR_NAME(iter));
        if (entry == NULL)
        {
            break;
        }
        iter = entry;
    }

    if (NODE_TYPE(cond) == NT_INT && NODE_TYPE(expr) == NT_INT &&
        (FORLOOP_ITER(node) == NULL || NODE_TYPE(iter) == NT_INT))
    {
        long start = INT_VAL(expr);
        long end = INT_VAL(cond);
        long step = iter == NULL ? 1 : INT_VAL(iter);

        if ((step > 0 && (start + step >= end)) || (step < 0 && (start + step <= end)))
        {
            // For loop is never executed. Remove the block so it becomes dead code
            CCNfree(FORLOOP_BLOCK(node));
            FORLOOP_BLOCK(node) = NULL;
            CCNcycleNotify();
        }
    }

    conditioned_assign = true;
    TRAVchildren(node);
    conditioned_assign = false;

    return node;
}

node_st *OPT_BEdowhileloop(node_st *node)
{
    node_st *expr = DOWHILELOOP_EXPR(node);
    while (NODE_TYPE(expr) == NT_VAR)
    {
        node_st *entry = HTlookup(assign_table, VAR_NAME(expr));
        if (entry == NULL)
        {
            break;
        }
        expr = entry;
    }

    if (NODE_TYPE(expr) == NT_BOOL && !BOOL_VAL(expr))
    {
        // Extract the block, so it becomes dead code.
        extracted_stmts_first = DOWHILELOOP_BLOCK(node);
        node_st *stmts = DOWHILELOOP_BLOCK(node);
        if (stmts != NULL)
        {
            while (STATEMENTS_NEXT(stmts) != NULL)
            {
                stmts = STATEMENTS_NEXT(stmts);
            }
        }
        extracted_stmts_last = stmts;
        DOWHILELOOP_BLOCK(node) = NULL;
        CCNcycleNotify();
    }

    TRAVchildren(node);
    return node;
}
