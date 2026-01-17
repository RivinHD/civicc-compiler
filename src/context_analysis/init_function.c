#include "ccngen/ast.h"
#include "context_analysis/definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "stdio.h"
#include "to_string.h"

#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>

static node_st *init_fun = NULL;
static uint32_t init_counter = 0;

node_st *CA_IFglobaldef(node_st *node)
{
    node_st *cur_vardec = GLOBALDEF_VARDEC(node);

    // VarDec
    node_st *var = VARDEC_VAR(cur_vardec);
    node_st *expr = VARDEC_EXPR(cur_vardec);

    if (expr != NULL)
    {
        node_st *init_funbody = FUNDEF_FUNBODY(init_fun);
        node_st *init_stmts = FUNBODY_STMTS(init_funbody);

        // init_fun -> FunBody -> Statements -> Statement -> Assign
        node_st *new_assign = NULL;

        if (NODE_TYPE(var) == NT_VAR)
        {
            new_assign = ASTassign(CCNcopy(var), CCNcopy(expr));
            VARDEC_EXPR(cur_vardec) = NULL;
        }
        else
        {
            // ArrayExpr

            // array assign: name[4,5] = 5;
            // int [1,2]array = 5;
            // ----
            // int [1,2]aray = tmp;
            // tmp = 5; // arrray assign
            // ----
            // for ...
            // array[i] = tmp;
            if (NODE_TYPE(expr) == NT_ARRAYINIT)
            {
                // Array Init

                // array[n, m]
                // unpack e.g. {{1, 2, 3,}, {4,5,6}}
                // array[0, 0] = 1;
                // array[0, 1] = 2;
                // ..
                // array[1, 2] = 6;
            }
            else
            {
                // Expr

                // Exprs
                // array[1,2] = 5;
                // ----
                // tmp = 5;
                // array[1,2] = tmp;
                // Step 3: New @init var
                char *tmp_name = STRfmt("@init%d", init_counter++);
                node_st *tmp_var = ASTvar(tmp_name);
                new_assign = ASTassign(tmp_var, CCNcopy(expr));

                // update current node
                VARDEC_EXPR(cur_vardec) = expr;
            }
        }
        node_st *new_stmts = ASTstatements(new_assign, init_stmts);
        FUNBODY_STMTS(init_funbody) = new_stmts;
    }

    TRAVopt(cur_vardec);

    return node;
}

node_st *CA_IFprogram(node_st *node)
{
    char *str = node_to_string(node);
    printf("%s\n", str);
    free(str);

    node_st *last_decls = PROGRAM_DECLS(node);
    release_assert(last_decls);
    while (DECLARATIONS_NEXT(last_decls) != NULL)
    {
        last_decls = DECLARATIONS_NEXT(last_decls);
    }
    release_assert(last_decls);

    // init function
    node_st *funheader = ASTfunheader(ASTvar(STRcpy(global_init_func)), NULL, DT_void);
    node_st *funbody = ASTfunbody(NULL, NULL, NULL);
    init_fun = ASTfundef(funheader, funbody, false);

    HTinsert(PROGRAM_SYMBOLS(node), global_init_func, init_fun);

    release_assert(DECLARATIONS_NEXT(last_decls) == NULL);
    node_st *init_decl = ASTdeclarations(init_fun, NULL);
    DECLARATIONS_NEXT(last_decls) = init_decl;

    TRAVopt(PROGRAM_DECLS(node));

    return node;
}
