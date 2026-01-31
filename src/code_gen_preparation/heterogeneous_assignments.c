#include "ccngen/ast.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <stdio.h>

static node_st *program_decls = NULL;
static node_st *last_fundef = NULL;
static uint32_t temp_counter = 0;
static htable_stptr current = NULL;
static node_st *last_vardecs = NULL;
static node_st *current_statements = NULL;
bool is_exported = false;

node_st *new_temp_assign(enum DataType type, node_st *expr)
{
    node_st *temp_var = ASTvar(STRfmt("@temp_%d", temp_counter++));
    node_st *temp_vardec = ASTvardec(CCNcopy(temp_var), NULL, type);
    HTinsert(current, VAR_NAME(VARDEC_VAR(temp_vardec)), temp_vardec);
    node_st *tmp_assign = ASTassign(CCNcopy(temp_var), expr);

    release_assert(last_fundef != NULL);
    node_st *cur_funbody = FUNDEF_FUNBODY(last_fundef);
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

    release_assert(current_statements != NULL);
    node_st *curr_stmt = STATEMENTS_STMT(current_statements);
    node_st *next_stmt = STATEMENTS_NEXT(current_statements);
    STATEMENTS_STMT(current_statements) = tmp_assign;
    node_st *next_stmts = ASTstatements(curr_stmt, next_stmt);
    STATEMENTS_NEXT(current_statements) = next_stmts;
    current_statements = next_stmts;

    return temp_var;
}

/**
 * Extract dimension expressions and substitute constants.
 *
 * Structure: array[ ... expr ... ] var;
 */
node_st *CGP_AAarrayexpr(node_st *node)
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

            // If the global def is exported we need dimension variables for module linkage
            if ((NODE_TYPE(cur_expr) == NT_INT && !is_exported) || NODE_TYPE(cur_expr) == NT_VAR)
            {
                exprs = EXPRS_NEXT(exprs);
                continue;
            }

            // If nested expr, traverse next
            TRAVopt(cur_expr);

            node_st *temp_var = NULL;
            release_assert(temp_counter != UINT32_MAX);
            // currently in global def (init fun)
            if (STReq(global_init_func, VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(last_fundef)))))
            {
                // Step 1: Create temp globalDef (without expr)
                temp_var = ASTvar(STRfmt("@temp_%d", temp_counter++));
                node_st *temp_vardec = ASTvardec(CCNcopy(temp_var), NULL, DT_int);
                node_st *temp_globaldef = ASTglobaldef(temp_vardec, false);
                HTinsert(current, VAR_NAME(VARDEC_VAR(temp_vardec)), temp_vardec);

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
                if (current_statements == NULL)
                {
                    // Inside Vardec
                    // Step 1: Create temp varDec
                    temp_var = ASTvar(STRfmt("@temp_%d", temp_counter++));
                    node_st *temp_vardec = ASTvardec(CCNcopy(temp_var), cur_expr, DT_int);
                    HTinsert(current, VAR_NAME(VARDEC_VAR(temp_vardec)), temp_vardec);

                    // Step 2: Append above of the current
                    if (last_vardecs == NULL)
                    {
                        node_st *temp_vardecs =
                            ASTvardecs(temp_vardec, FUNBODY_VARDECS(cur_funbody));
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
                else
                {
                    // Inside assignment
                    temp_var = new_temp_assign(DT_int, cur_expr);
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

node_st *CGP_AAforloop(node_st *node)
{

    TRAVopt(FORLOOP_ASSIGN(node));
    TRAVopt(FORLOOP_COND(node));
    TRAVopt(FORLOOP_ITER(node));

    node_st *assign = FORLOOP_ASSIGN(node);
    if (NODE_TYPE(ASSIGN_EXPR(assign)) != NT_INT)
    {
        node_st *var = new_temp_assign(DT_int, ASSIGN_EXPR(assign));
        ASSIGN_EXPR(assign) = var;
    }

    if (NODE_TYPE(FORLOOP_COND(node)) != NT_INT ||
        (FORLOOP_ITER(node) != NULL && NODE_TYPE(FORLOOP_ITER(node)) != NT_INT))
    {
        // if iter is a var than cond need to be a var too. See code gen general case
        node_st *var = new_temp_assign(DT_int, FORLOOP_COND(node));
        FORLOOP_COND(node) = var;
    }

    if (FORLOOP_ITER(node) != NULL && NODE_TYPE(FORLOOP_ITER(node)) != NT_INT)
    {
        node_st *var = new_temp_assign(DT_int, FORLOOP_ITER(node));
        FORLOOP_ITER(node) = var;
    }

    TRAVopt(FORLOOP_BLOCK(node));
    return node;
}

node_st *CGP_AAstatements(node_st *node)
{
    node_st *parent_current_stmts = current_statements;
    current_statements = node;
    TRAVopt(STATEMENTS_STMT(current_statements));
    node_st *this_stmts = current_statements;
    TRAVopt(STATEMENTS_NEXT(this_stmts));

    current_statements = parent_current_stmts;
    return node;
}

node_st *CGP_AAvardecs(node_st *node)
{
    TRAVopt(VARDECS_VARDEC(node));
    last_vardecs = node;
    TRAVopt(VARDECS_NEXT(node));
    return node;
}

node_st *CGP_AAfunbody(node_st *node)
{
    // Do not remove otherwise the changes are not applied.
    TRAVopt(FUNBODY_VARDECS(node));
    TRAVopt(FUNBODY_LOCALFUNDEFS(node));
    TRAVopt(FUNBODY_STMTS(node));

    return node;
}

node_st *CGP_AAglobaldef(node_st *node)
{
    is_exported = GLOBALDEF_HAS_EXPORT(node);
    TRAVopt(GLOBALDEF_VARDEC(node));
    is_exported = false;
    return node;
}

/**
 * For symbol table.
 */
node_st *CGP_AAfundef(node_st *node)
{
    node_st *parent_fundef = last_fundef;
    node_st *parent_last_vardecs = last_vardecs;
    last_vardecs = NULL;

    current = FUNDEF_SYMBOLS(node);
    release_assert(current != NULL);

    last_fundef = node;
    temp_counter = 0;

    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));

    current = HTlookup(current, htable_parent_name);
    release_assert(current != NULL);
    last_fundef = parent_fundef;
    last_vardecs = parent_last_vardecs;

    return node;
}

/**
 * For symbol table.
 */
node_st *CGP_AAprogram(node_st *node)
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
