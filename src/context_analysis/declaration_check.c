#include "ccngen/ast.h"
#include "definitions.h"
#include "palm/ctinfo.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>

static htable_stptr current = NULL;
static node_st *program_node = NULL;

static void reset_state()
{
    current = NULL;
    program_node = NULL;
}

void add_var_symbol(node_st *node, node_st *var)
{
    release_assert(node != NULL);
    release_assert(var != NULL);
    enum ccn_nodetype type = NODE_TYPE(var);
    release_assert(type == NT_VAR || type == NT_ARRAYVAR || type == NT_ARRAYEXPR);

    char *name = NULL;
    switch (NODE_TYPE(var))
    {
    case NT_VAR:
        name = VAR_NAME(var);
        break;
    case NT_ARRAYVAR:
        name = VAR_NAME(ARRAYVAR_VAR(var));
        break;
    case NT_ARRAYEXPR:
        name = VAR_NAME(ARRAYEXPR_VAR(var));
        break;
    default:
        release_assert(false);
        return;
    }

    const char *pretty_name = get_pretty_name(name);
    node_st *entry = HTlookup(current, name);
    if (entry == NULL)
    {
        if (name[0] == '_')
        {
            error_invalid_identifier_name(node, entry, pretty_name);
        }

        bool success = HTinsert(current, name, node);
        release_assert(success);
    }
    else
    {
        error_already_defined(node, entry, pretty_name);
    }
}

node_st *CA_DCproccall(node_st *node)
{
    node_st *var = PROCCALL_VAR(node);
    char *name = VAR_NAME(var);

    VAR_NAME(var) = STRfmt("@fun_%s", name);
    free(name);

    TRAVopt(var);
    TRAVopt(PROCCALL_EXPRS(node));
    return node;
}

node_st *CA_DCvar(node_st *node)
{
    char *name = VAR_NAME(node);

    if (name != NULL && name[0] == '_')
    {
        error_invalid_identifier_name(node, node, name);
    }
    node_st *entry = deep_lookup(current, name);
    if (entry == NULL)
    {
        struct ctinfo info = NODE_TO_CTINFO(node);
        info.filename = STRcpy(global.input_file);
        CTIobj(CTI_ERROR, true, info, "'%s' was not declared.", get_pretty_name(name));
        free(info.filename);
    }
    return node;
}

node_st *CA_DCvardec(node_st *node)
{
    // Edge case int a = a + 1 // a in expr is not declared
    TRAVopt(VARDEC_EXPR(node));
    node_st *var = VARDEC_VAR(node);
    if (NODE_TYPE(var) == NT_ARRAYEXPR)
    {
        TRAVopt(ARRAYEXPR_DIMS(var));
        add_var_symbol(node, var);
        TRAVopt(ARRAYEXPR_VAR(var));
    }
    else
    {
        release_assert(NODE_TYPE(var) == NT_VAR);
        add_var_symbol(node, var);
        TRAVopt(var);
    }
    return node;
}

node_st *CA_DCarrayexpr(node_st *node)
{
    TRAVopt(ARRAYEXPR_DIMS(node));
    TRAVopt(ARRAYEXPR_VAR(node));
    return node;
}

node_st *CA_DCparams(node_st *node)
{
    node_st *var = PARAMS_VAR(node);
    add_var_symbol(node, var);

    TRAVopt(PARAMS_VAR(node));
    TRAVopt(PARAMS_NEXT(node));
    return node;
}

node_st *CA_DCglobaldec(node_st *node)
{
    node_st *var = GLOBALDEC_VAR(node);
    add_var_symbol(node, var);

    TRAVopt(GLOBALDEC_VAR(node));
    return node;
}

node_st *CA_DCfundec(node_st *node)
{
    htable_stptr parent_current = current;
    current = HTnew_String(2 << 8);
    bool success = HTinsert(current, htable_parent_name, parent_current);
    release_assert(success);
    TRAVopt(FUNDEC_FUNHEADER(node));
    HTdelete(current);
    current = parent_current;
    return node;
}

node_st *CA_DCdimensionvars(node_st *node)
{
    node_st *var = DIMENSIONVARS_DIM(node);
    add_var_symbol(node, var);

    TRAVopt(var);
    TRAVopt(DIMENSIONVARS_NEXT(node));
    return node;
}

node_st *CA_DCfundef(node_st *node)
{
    current = FUNDEF_SYMBOLS(node);
    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));
    current = HTlookup(current, htable_parent_name);
    release_assert(current != NULL);

    return node;
}

node_st *CA_DCdeclarations(node_st *node)
{
    node_st *decl = DECLARATIONS_DECL(node);
    if (NODE_TYPE(decl) == NT_GLOBALDEC || NODE_TYPE(decl) == NT_GLOBALDEF)
    {
        TRAVopt(decl); // Traverse global decs in there orderd
        TRAVopt(DECLARATIONS_NEXT(node));
    }
    else
    {
        release_assert(NODE_TYPE(decl) == NT_FUNDEF || NODE_TYPE(decl) == NT_FUNDEC);
        TRAVopt(DECLARATIONS_NEXT(node));
        TRAVopt(decl); // Traverse functions after all globals
    }
    return node;
}

node_st *CA_DCprogram(node_st *node)
{
    reset_state();
    current = PROGRAM_SYMBOLS(node);
    program_node = node;

    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
