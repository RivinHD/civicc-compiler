#include "ccngen/ast.h"
#include "context_analysis/definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>

static node_st *init_fun = NULL;

node_st *CA_IFglobaldef(node_st *node)
{
    node_st *cur_vardec = GLOBALDEF_VARDEC(node);

    node_st *expr = VARDEC_EXPR(cur_vardec);
    node_st *var = VARDEC_VAR(cur_vardec);

    // init_fun -> FunBody -> Statements -> Statement -> Assign
    node_st *new_assign = ASTassign(var, expr);
    node_st *new_stmts = ASTstatements(new_assign, NULL);

    node_st *init_funbody = FUNDEF_FUNBODY(init_fun);
    node_st *init_stmts = FUNBODY_STMTS(init_funbody);
    STATEMENTS_NEXT(init_stmts) = new_stmts;

    // update current node
    VARDEC_EXPR(cur_vardec) = NULL;

    return node;
}

node_st *CA_IFprogram(node_st *node)
{
    node_st *cur_decls = PROGRAM_DECLS(node);
    release_assert(cur_decls);
    while (DECLARATIONS_NEXT(cur_decls) != NULL)
    {
        cur_decls = DECLARATIONS_NEXT(cur_decls);
    }
    release_assert(cur_decls);

    // init function
    node_st *funheader = ASTfunheader(ASTvar("__init"), NULL, DT_void);
    node_st *funbody = ASTfunbody(NULL, NULL, NULL);
    init_fun = ASTfundef(funheader, funbody, false);

    HTinsert(PROGRAM_SYMBOLS(node), global_init_func, init_fun);

    node_st *init_decl = ASTdeclarations(init_fun, NULL);
    DECLARATIONS_NEXT(cur_decls) = init_decl;

    return node;
}