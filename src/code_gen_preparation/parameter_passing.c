#include "ccngen/ast.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"

#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>

static htable_stptr current = NULL;

node_st *CGP_PPproccall(node_st *node)
{
    char *procall_name = VAR_NAME(PROCCALL_VAR(node));
    if (STReq(procall_name, alloc_func))
    {
        // builtin function @alloc is called
        TRAVchildren(node);
        return node;
    }

    node_st *proccall_exprs = PROCCALL_EXPRS(node);
    while (proccall_exprs != NULL)
    {
        node_st *cur_expr = EXPRS_EXPR(proccall_exprs);

        if (NODE_TYPE(cur_expr) == NT_PROCCALL)
        {
            TRAVopt(PROCCALL_EXPRS(cur_expr));
        }
        else if (NODE_TYPE(cur_expr) == NT_CAST)
        {
            TRAVopt(CAST_EXPR(cur_expr));
        }
        else if (NODE_TYPE(cur_expr) == NT_ARRAYEXPR)
        {
            TRAVopt(EXPRS_EXPR(ARRAYEXPR_DIMS(cur_expr)));
            TRAVopt(EXPRS_NEXT(ARRAYEXPR_DIMS(cur_expr)));
        }
        else if (NODE_TYPE(cur_expr) == NT_VAR)
        {
            node_st *entry = deep_lookup(current, VAR_NAME(cur_expr));
            release_assert(entry != NULL);
            node_st *cur_var = get_var_from_symbol(entry);

            if (NODE_TYPE(cur_var) == NT_ARRAYVAR)
            {
                node_st *cur_arrayvar_dim_vars = ARRAYVAR_DIMS(cur_var);
                while (cur_arrayvar_dim_vars != NULL)
                {
                    node_st *new_exprs = ASTexprs(CCNcopy(DIMENSIONVARS_DIM(cur_arrayvar_dim_vars)),
                                                  PROCCALL_EXPRS(node));
                    PROCCALL_EXPRS(node) = new_exprs;
                    cur_arrayvar_dim_vars = DIMENSIONVARS_NEXT(cur_arrayvar_dim_vars);
                }
            }
            else if(NODE_TYPE(cur_var) == NT_ARRAYEXPR)
            {
                node_st *cur_arrayexpr_dim_vars = ARRAYEXPR_DIMS(cur_var);
                while (cur_arrayexpr_dim_vars != NULL)
                {
                    node_st *new_exprs = ASTexprs(CCNcopy(EXPRS_EXPR(cur_arrayexpr_dim_vars)),
                                                  PROCCALL_EXPRS(node));
                    PROCCALL_EXPRS(node) = new_exprs;
                    cur_arrayexpr_dim_vars = EXPRS_NEXT(cur_arrayexpr_dim_vars);
                }
            }
        }

        proccall_exprs = EXPRS_NEXT(proccall_exprs);
    }

    return node;
}

node_st *CGP_PPfundef(node_st *node)
{
    // Set current symbol table
    current = FUNDEF_SYMBOLS(node);
    release_assert(current != NULL);

    node_st *cur_funheader = FUNDEF_FUNHEADER(node);
    node_st *cur_params = FUNHEADER_PARAMS(cur_funheader);

    while (cur_params != NULL)
    {
        node_st *cur_var = PARAMS_VAR(cur_params);
        if (NODE_TYPE(cur_var) == NT_ARRAYVAR)
        {
            node_st *cur_dim_var = ARRAYVAR_DIMS(cur_var);

            while (cur_dim_var != NULL)
            {
                node_st *new_params = ASTparams(CCNcopy(DIMENSIONVARS_DIM(cur_dim_var)),
                                                FUNHEADER_PARAMS(cur_funheader), DT_int);
                FUNHEADER_PARAMS(cur_funheader) = new_params;
                cur_dim_var = DIMENSIONVARS_NEXT(cur_dim_var);
            }
        }

        cur_params = PARAMS_NEXT(cur_params);
    }

    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));

    // reset symbol table
    current = HTlookup(current, htable_parent_name);

    return node;
}

node_st *CGP_PPprogram(node_st *node)
{
    TRAVopt(PROGRAM_DECLS(node));

    return node;
}
