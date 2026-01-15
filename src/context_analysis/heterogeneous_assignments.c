#include "ccngen/ast.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>

static node_st *last_funbody = NULL;
static uint32_t temp_counter = 0;
static htable_stptr current = NULL;

/**
 * Extract expressions from array assignments.
 *
 * Structure: array[...] = ... expr ...;
 */
node_st *CA_AAarrayassign(node_st *node)
{
    release_assert(last_funbody);

    if (NODE_TYPE(node) != NT_VAR)
    {
        // DataType Lookup
        node_st *arrayassign_var = ARRAYASSIGN_VAR(node);
        node_st *temp_arrayvar = deep_lookup(current, VAR_NAME(arrayassign_var));
        const enum DataType var_type = symbol_to_type(temp_arrayvar);

        // Update (temp) expr -> var
        node_st *assign_expr = ARRAYASSIGN_EXPR(node);

        node_st *temp_var = ASTvar(STRfmt("@temp_%d", temp_counter++));
        node_st *temp_vardec = ASTvardec(temp_var, assign_expr, var_type);
        node_st *temp_vardecs = ASTvardecs(temp_vardec, VARDECS_NEXT(last_funbody));
        VARDECS_NEXT(last_funbody) = temp_vardecs;

        // Update (node) expr -> var
        ARRAYASSIGN_EXPR(node) = CCNcopy(temp_var);
    }

    return node;
}

/**
 * Extract dimension expressions and substitute constants.
 *
 * Structure: array[ ... expr ... ] var;
 */
node_st *CA_AAarrayexpr(node_st *node)
{
    release_assert(last_funbody);

    if (ARRAYEXPR_DIMS(node) != NULL)
    {
        node_st *exprs = ARRAYEXPR_DIMS(node);

        while (EXPRS_NEXT(exprs) != NULL)
        {
            node_st *cur_expr = EXPRS_EXPR(exprs);

            // First: Extract
            node_st *temp_var = ASTvar(STRfmt("@temp_%d", temp_counter++));
            node_st *temp_vardec = ASTvardec(temp_var, cur_expr, DT_int);
            // Append to the front
            node_st *temp_vardecs = ASTvardecs(temp_vardec, VARDECS_NEXT(last_funbody));
            VARDECS_NEXT(last_funbody) = temp_vardecs;

            EXPRS_EXPR(exprs) = CCNcopy(temp_var);
            exprs = EXPRS_NEXT(exprs);

            // If nested expr, traverse next
            if (NODE_TYPE(cur_expr) == NT_ARRAYEXPR)
            {
                TRAVopt(cur_expr);
            }
        }
    }

    return node;
}

/**
 * Place to store extracted expr
 */
node_st *CA_AAfunbody(node_st *node)
{
    last_funbody = node;
    temp_counter = 0;
    return node;
}

/**
 * For symbol table.
 */
node_st *CA_AAfundef(node_st *node)
{
    current = FUNDEF_SYMBOLS(node);
    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));
    current = HTlookup(current, "@parent");
    release_assert(current != NULL);

    return node;
}

/**
 * For symbol table.
 */
node_st *CA_AAprogram(node_st *node)
{
    current = PROGRAM_SYMBOLS(node);

    // Global arrays unpack assignment into the __init function
    node_st* entry = HTlookup(current, global_init_func);
    release_assert(entry != NULL);
    release_assert(NODE_TYPE(entry) == NT_FUNDEF);
    last_funbody = FUNDEF_FUNBODY(entry);


    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
