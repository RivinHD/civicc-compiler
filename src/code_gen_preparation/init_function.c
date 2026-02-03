#include "ccngen/ast.h"
#include "code_gen_preparation/unpack_arrayinit.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "stdio.h"
#include "utils.h"

#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>

static node_st *init_fun = NULL;
static node_st *init_stmts = NULL;

static void reset_state()
{
    init_fun = NULL;
    init_stmts = NULL;
}

node_st *CGP_IFglobaldef(node_st *node)
{
    node_st *cur_vardec = GLOBALDEF_VARDEC(node);

    // VarDec
    node_st *var = VARDEC_VAR(cur_vardec);
    node_st *expr = VARDEC_EXPR(cur_vardec);

    if (expr != NULL)
    {
        node_st *init_funbody = FUNDEF_FUNBODY(init_fun);

        // init_fun -> FunBody -> Statements -> Statement -> Assign
        node_st *new_assign = NULL;

        if (NODE_TYPE(var) == NT_VAR)
        {
            new_assign = ASTassign(CCNcopy(var), expr);
            VARDEC_EXPR(cur_vardec) = NULL;
            init_stmts = add_stmt(new_assign, init_stmts, init_funbody);
        }
    }

    return node;
}

node_st *CGP_IFprogram(node_st *node)
{
    reset_state();
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
    init_fun = ASTfundef(funheader, funbody, true);
    FUNDEF_SYMBOLS(init_fun) = HTnew_String(2 << 8);
    htable_stptr symbols = FUNDEF_SYMBOLS(init_fun);
    bool success = HTinsert(symbols, htable_parent_name, PROGRAM_SYMBOLS(node));
    release_assert(success);

    success = HTinsert(PROGRAM_SYMBOLS(node), global_init_func, init_fun);
    release_assert(success);

    release_assert(DECLARATIONS_NEXT(last_decls) == NULL);
    node_st *init_decl = ASTdeclarations(init_fun, NULL);
    DECLARATIONS_NEXT(last_decls) = init_decl;

    TRAVopt(PROGRAM_DECLS(node));

    return node;
}
