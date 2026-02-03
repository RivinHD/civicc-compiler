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
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

static htable_stptr current = NULL;
static uint32_t level = 0;

// TODO fix heap use after free
static void reset_state()
{
    current = NULL;
    level = 0;
}

static char *no_level_name(char *name)
{
    if (STRprefix("@", name) && !STRprefix("@fun_", name) && !STRprefix("@for", name))
    {
        unsigned int start = 1; // Skip @ at start
        while (name[start] != '_' && name[start] != '\0')
        {
            release_assert(isdigit(name[start]));
            start++;
        }
        start++; // Consume the '_' too

        release_assert(name[start] != '\0');
        return name + start;
    }
    return name;
}

static void sync_symbol_keys(htable_stptr symbols)
{
    size_t size = HTelementCount(symbols);
    char **delete_names = malloc(size * sizeof(char *));
    size_t size_names = 0;
    for (htable_iter_st *iter = HTiterate(symbols); iter; iter = HTiterateNext(iter))
    {
        char *name = HTiterKey(iter);
        if (STRprefix("@fun", name) || STReq(htable_parent_name, name))
        {
            // Skip all function declarations and the parent
            continue;
        }

        delete_names[size_names++] = name;
    }

    for (size_t i = 0; i < size_names; i++)
    {

        char *name = delete_names[i];

        node_st *entry = HTlookup(symbols, name);
        release_assert(entry != NULL);
        node_st *var = get_var_from_symbol(entry);
        char *entry_name;
        switch (NODE_TYPE(var))
        {
        case NT_VAR:
            entry_name = VAR_NAME(var);
            break;
        case NT_ARRAYEXPR:
            entry_name = VAR_NAME(ARRAYEXPR_VAR(var));
            break;
        case NT_ARRAYVAR:
            entry_name = VAR_NAME(ARRAYVAR_VAR(var));
            break;
        default:
            release_assert(false);
            break;
        }

        if (STReq(name, entry_name))
        {
            continue;
        }

        bool success = HTinsert(symbols, entry_name, entry);
        release_assert(success);
        node_st *value = HTremove(symbols, name);
        free(name);
        release_assert(value != NULL);
        release_assert(entry == value);
    }
    free(delete_names);
}

static void add_var_symbol(node_st *node, node_st *var)
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

    bool has_new_name = false;
    if (!STRprefix("@", name))
    {
        has_new_name = true;
        // added level specific command_name
        char *new_name = STRfmt("@%d_%s", level, name);
        switch (NODE_TYPE(var))
        {
        case NT_VAR:
            VAR_NAME(var) = new_name;
            break;
        case NT_ARRAYVAR:
            VAR_NAME(ARRAYVAR_VAR(var)) = new_name;
            break;
        case NT_ARRAYEXPR:
            VAR_NAME(ARRAYEXPR_VAR(var)) = new_name;
            break;
        default:
            release_assert(false);
            return;
        }
    }

    const char *pretty_name = get_pretty_name(name);
    // We set it with is old name in the symbol table to be then found by the expressions
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
        if (has_new_name)
        {
            free(name);
        }
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
    char *true_name = VAR_NAME(node);
    char *name = no_level_name(true_name);

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
    else if (!STRprefix("@fun", true_name))
    {
        // Rename to same name as entry
        node_st *var = get_var_from_symbol(entry);
        char *entry_name;
        switch (NODE_TYPE(var))
        {
        case NT_VAR:
            entry_name = VAR_NAME(var);
            break;
        case NT_ARRAYEXPR:
            entry_name = VAR_NAME(ARRAYEXPR_VAR(var));
            break;
        case NT_ARRAYVAR:
            entry_name = VAR_NAME(ARRAYVAR_VAR(var));
            break;
        default:
            release_assert(false);
            break;
        }

        if (!STReq(true_name, entry_name))
        {
            VAR_NAME(node) = STRcpy(entry_name);
            free(true_name);
        }
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
    sync_symbol_keys(current);
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
    uint32_t parent_level = level;
    current = FUNDEF_SYMBOLS(node);
    level++;
    TRAVopt(FUNDEF_FUNHEADER(node));
    TRAVopt(FUNDEF_FUNBODY(node));
    sync_symbol_keys(current);
    current = HTlookup(current, htable_parent_name);
    release_assert(current != NULL);
    level = parent_level;
    return node;
}

node_st *CA_DCdeclarations(node_st *node)
{
    node_st *decl = DECLARATIONS_DECL(node);
    if (NODE_TYPE(decl) == NT_GLOBALDEC || NODE_TYPE(decl) == NT_GLOBALDEF ||
        NODE_TYPE(decl) == NT_FUNDEC)
    {
        TRAVopt(decl); // Traverse global decs in there orderd
        TRAVopt(DECLARATIONS_NEXT(node));
    }
    else
    {
        release_assert(NODE_TYPE(decl) == NT_FUNDEF);
        TRAVopt(DECLARATIONS_NEXT(node));
        TRAVopt(decl); // Traverse functions after all globals
    }
    return node;
}

node_st *CA_DCprogram(node_st *node)
{
    reset_state();
    current = PROGRAM_SYMBOLS(node);

    TRAVopt(PROGRAM_DECLS(node));

    sync_symbol_keys(current);
    htable_stptr parent = HTlookup(current, htable_parent_name);
    release_assert(parent == NULL);
    return node;
}
