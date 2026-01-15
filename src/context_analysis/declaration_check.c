#include "ccngen/ast.h"
#include "ccngen/trav.h"
#include "context_analysis/definitions.h"
#include "palm/ctinfo.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "to_string.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <stdio.h>

static htable_stptr current = NULL;

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

        HTinsert(current, name, node);
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
    node_st *var = VARDEC_VAR(node);
    add_var_symbol(node, var);
    TRAVopt(VARDEC_VAR(node));
    TRAVopt(VARDEC_EXPR(node));
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
    // No need to check fundec
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
    current = HTlookup(current, "@parent");
    release_assert(current != NULL);

    return node;
}

node_st *CA_DCprogram(node_st *node)
{
    current = PROGRAM_SYMBOLS(node);

    TRAVopt(PROGRAM_DECLS(node));
    return node;
}
