#include "ccngen/ast.h"
#include "context_analysis/definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "to_string.h"
#include "user_types.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdio.h>

static node_st *program_decls = NULL;
static node_st *last_fundef = NULL;
static uint32_t temp_counter = 0;
static htable_stptr current = NULL;
static node_st *last_vardecs = NULL;

/**
 * Extract dimension expressions and substitute constants.
 *
 * Structure: array[ ... expr ... ] var;
 */
node_st *CA_AAarrayexpr(node_st *node)
{
    release_assert(last_fundef);

    if (ARRAYEXPR_DIMS(node) != NULL)
    {
        node_st *exprs = ARRAYEXPR_DIMS(node);
        node_st *cur_funbody = FUNDEF_FUNBODY(last_fundef);

        while (exprs != NULL)
        {
            node_st *cur_expr = EXPRS_EXPR(exprs);
            release_assert(cur_expr != NULL);

            if (NODE_TYPE(cur_expr) == NT_INT || NODE_TYPE(cur_expr) == NT_VAR)
            {
                exprs = EXPRS_NEXT(exprs);
                continue;
            }

            // If nested expr, traverse next
            TRAVopt(cur_expr);

            node_st *temp_var = ASTvar(STRfmt("@temp_%d", temp_counter++));
            release_assert(temp_counter != UINT32_MAX);
            // currently in global def (init fun)
            if (STReq(global_init_func, VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(last_fundef)))))
            {
                // Step 1: Create temp globalDef (without expr)
                node_st *temp_vardec = ASTvardec(CCNcopy(temp_var), NULL, DT_int);
                node_st *temp_globaldef = ASTglobaldef(temp_vardec, false);

                // Step 2: Append to before the current globalDef (decls)
                node_st *temp_decls = ASTdeclarations(temp_globaldef, program_decls);
                program_decls = temp_decls;

                // Step 3: Add varAssign to init fun (+ expr)
                node_st *new_assign = ASTassign(CCNcopy(temp_var), cur_expr);
                node_st *new_stmts = ASTstatements(new_assign, FUNBODY_STMTS(cur_funbody));
                FUNBODY_STMTS(cur_funbody) = new_stmts;
            }
            else
            {
                // Step 1: Create temp varDec
                node_st *temp_vardec = ASTvardec(CCNcopy(temp_var), cur_expr, DT_int);

                // Step 2: Append above of the current
                if (last_vardecs == NULL)
                {
                    node_st *temp_vardecs = ASTvardecs(temp_vardec, FUNBODY_VARDECS(cur_funbody));
                    FUNBODY_VARDECS(cur_funbody) = temp_vardecs;
                    last_vardecs = temp_vardecs;
                }
                else
                {
                    node_st *temp_vardecs = ASTvardecs(temp_vardec, VARDECS_NEXT(last_vardecs));
                    VARDECS_NEXT(last_vardecs) = temp_vardecs;
                    last_vardecs = temp_vardecs;
                }
            }

            EXPRS_EXPR(exprs) = temp_var;

            exprs = EXPRS_NEXT(exprs);
        }
    }

    TRAVopt(ARRAYEXPR_DIMS(node));
    TRAVopt(ARRAYEXPR_VAR(node));

    return node;
}

node_st *CA_AAvardecs(node_st *node)
{
    TRAVopt(VARDECS_VARDEC(node));
    last_vardecs = node;
    TRAVopt(VARDECS_NEXT(node));

    return node;
}

node_st *CA_AAfunbody(node_st *node)
{
    // Do not remove otherwise the changes are not applied.
    TRAVopt(FUNBODY_VARDECS(node));
    TRAVopt(FUNBODY_LOCALFUNDEFS(node));
    TRAVopt(FUNBODY_STMTS(node));

    return node;
}

/**
 * For symbol table.
 */
node_st *CA_AAfundef(node_st *node)
{
    node_st *parent_fundef = last_fundef;
    last_vardecs = NULL;
    bool is_init_function =
        STReq(VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(node))), global_init_func);

    if (is_init_function)
    {
        release_assert(FUNDEF_SYMBOLS(node) == NULL);
    }
    else
    {
        current = FUNDEF_SYMBOLS(node);
        release_assert(current != NULL);
    }

    last_fundef = node;
    temp_counter = 0;

    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));

    if (!is_init_function)
    {
        current = HTlookup(current, htable_parent_name);
    }
    release_assert(current != NULL);
    last_fundef = parent_fundef;

    return node;
}

/**
 * For symbol table.
 */
node_st *CA_AAprogram(node_st *node)
{
    current = PROGRAM_SYMBOLS(node);
    program_decls = PROGRAM_DECLS(node);

    // Global arrays unpack assignment into the __init function
    node_st *entry = HTlookup(current, global_init_func);
    release_assert(entry != NULL);
    release_assert(NODE_TYPE(entry) == NT_FUNDEF);
    last_fundef = entry;

    TRAVopt(PROGRAM_DECLS(node));

    PROGRAM_DECLS(node) = program_decls;

    return node;
}
