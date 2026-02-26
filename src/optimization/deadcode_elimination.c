#include "ccngen/ast.h"
#include "definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccn/phase_driver.h>
#include <ccngen/enum.h>
#include <endian.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

static htable_stptr current = NULL;
static htable_stptr sideeffect_table = NULL;
static htable_stptr usage_table = NULL;
static node_st *sideeffect_stmts_first = NULL;
static node_st *sideeffect_stmts_last = NULL;
static bool collect_sideeffects = false;
static bool check_consumtion = false;
static bool has_consumtion = false;
static htable_stptr if_usage_stmts = NULL;
static htable_stptr else_usage_stmts = NULL;
static bool collect_if_usages = false;
static bool collect_else_usages = false;
static htable_stptr init_symbols = NULL;
static bool remove_local_function = true;
static bool check_return = false;
static bool has_return = false;
static bool tag_usages = false;

static void reset()
{
    current = NULL;
    sideeffect_table = NULL;
    usage_table = NULL;
    sideeffect_stmts_first = NULL;
    sideeffect_stmts_last = NULL;
    collect_sideeffects = false;
    check_consumtion = false;
    has_consumtion = false;
    if_usage_stmts = NULL;
    else_usage_stmts = NULL;
    collect_if_usages = false;
    collect_else_usages = false;
    init_symbols = NULL;
    remove_local_function = false;
    check_return = false;
    has_return = false;
    tag_usages = false;
}

enum usage_state
{
    UC_NONE = 0, // always MIN
    UC_CONSUMED = 1,
    UC_USAGE = 2, // always MAX
};

// Shift to not overwrite the value of usage_state MAX
const uint UCshift = 5;
const uint UCmask = (1 << UCshift) - 1;

static void UCset(void *key, enum usage_state state)
{
    release_assert(usage_table != NULL);
    release_assert(key != NULL);
    HTremove(usage_table, key);
    bool success = HTinsert(usage_table, key, (void *)state);
    release_assert(success);
}

static enum usage_state UClookup(void *key)
{
    release_assert(usage_table != NULL);
    release_assert(key != NULL);
    void *entry = HTlookup(usage_table, key);
    ptrdiff_t value = (ptrdiff_t)entry;
    release_assert(value >= UC_NONE);
    release_assert(value <= UC_USAGE);
    return (enum usage_state)value;
}

static void UCrestore()
{
    release_assert(usage_table != NULL);
    if (if_usage_stmts == NULL)
    {
        release_assert(else_usage_stmts == NULL);
        return;
    }

    for (htable_iter_st *iter = HTiterate(if_usage_stmts); iter; iter = HTiterateNext(iter))
    {
        void *key = HTiterKey(iter);
        void *value = HTiterValue(iter);
        ptrdiff_t if_value = (ptrdiff_t)value & UCmask;
        release_assert(if_value >= UC_NONE);
        release_assert(if_value <= UC_USAGE);
        enum usage_state if_state = (enum usage_state)if_value;

        void *entry = HTlookup(else_usage_stmts, key);
        ptrdiff_t else_value = (ptrdiff_t)entry & UCmask;
        release_assert(else_value >= UC_NONE);
        release_assert(else_value <= UC_USAGE);
        enum usage_state else_state = (enum usage_state)else_value;

        if ((if_state == UC_CONSUMED) & (else_state == UC_CONSUMED))
        {
            // We keep whats inside the usage value, as it is correct.
            release_assert(UClookup(key) == UC_CONSUMED);
        }
        else
        {
            // We need to restore
            // Its always usage or consumed, as it also was used the the statement itself. Thus, we
            // need the var decleration to exist.
            ptrdiff_t if_value_origin = ((ptrdiff_t)value >> UCshift) & UCmask;
            release_assert(if_value_origin >= UC_NONE);
            release_assert(if_value_origin <= UC_USAGE);
            if (entry != NULL)
            {
                ptrdiff_t else_value_origin = ((ptrdiff_t)entry >> UCshift) & UCmask;
                release_assert(else_value_origin >= UC_NONE);
                release_assert(else_value_origin <= UC_USAGE);
                // Should have the same origin i.e. we want to keep the vardec
                release_assert(if_value_origin == else_value_origin);
            }

            // If we found a value its at least consumed
            enum usage_state origin = (enum usage_state)if_value_origin;
            origin = origin == UC_NONE ? UC_CONSUMED : origin;
            UCset(key, origin);
        }
    }

    // Now restore the remaining that the else contains but not if
    for (htable_iter_st *iter = HTiterate(else_usage_stmts); iter; iter = HTiterateNext(iter))
    {
        void *key = HTiterKey(iter);
        void *value = HTiterValue(iter);
        ptrdiff_t else_value = (ptrdiff_t)value & UCmask;
        release_assert(else_value >= UC_NONE);
        release_assert(else_value <= UC_USAGE);

        if (HTlookup(if_usage_stmts, key) == NULL)
        {
            ptrdiff_t else_value_origin = ((ptrdiff_t)value >> UCshift) & UCmask;
            release_assert(else_value_origin >= UC_NONE);
            release_assert(else_value_origin <= UC_USAGE);
            // If we found a value its at least consumed i.e. we want to keep the vardec
            enum usage_state origin = (enum usage_state)else_value_origin;
            origin = origin == UC_NONE ? UC_CONSUMED : origin;
            UCset(key, origin);
        }
    }
}

/// Checks if a node has a any asignment statement
static bool has_consume(node_st *node)
{
    if (node == NULL)
    {
        return false;
    }

    has_consumtion = false;
    check_consumtion = true;
    TRAVopt(node);
    check_consumtion = false;
    return has_consumtion;
}

static bool check_stmts_sideffect(node_st *stmts, htable_stptr symbols)
{
    if (stmts == NULL)
    {
        return false;
    }

    release_assert(NODE_TYPE(stmts) == NT_STATEMENTS);
    release_assert(symbols != NULL);

    enum sideeffect effect = SEFF_NO;
    while (stmts != NULL)
    {

        enum sideeffect eff =
            check_stmt_sideeffect(STATEMENTS_STMT(stmts), symbols, sideeffect_table);
        release_assert(eff != SEFF_NULL);
        release_assert(eff != SEFF_PROCESSING);
        effect |= eff;
        stmts = STATEMENTS_NEXT(stmts);
    }

    return effect == SEFF_YES;
}

/* Idea: Start backwards from the return statements / or last statement of a void function.
 * On the forward pass, add the usage count of a vairable/fundef and remove unused stuff in
 * the backward pass.
 */

node_st *OPT_DCEprogram(node_st *node)
{
    reset();
    current = PROGRAM_SYMBOLS(node);
    sideeffect_table = HTnew_Ptr(2 << 8);
    usage_table = HTnew_Ptr(2 << 8);
    TRAVchildren(node);
    HTdelete(sideeffect_table);
    HTdelete(usage_table);
    return node;
}

node_st *OPT_DCEfundef(node_st *node)
{
    release_assert(collect_sideeffects == false);
    release_assert(tag_usages == false);

    htable_stptr parent_current = current;
    current = FUNDEF_SYMBOLS(node);

    // Parameters that are arrays are reference, thus we need to assume they have a usage.
    node_st *param = FUNHEADER_PARAMS(FUNDEF_FUNHEADER(node));
    while (param != NULL)
    {
        if (NODE_TYPE(PARAMS_VAR(param)) == NT_ARRAYVAR)
        {
            UCset(param, UC_USAGE);
        }

        param = PARAMS_NEXT(param);
    }

    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVopt(FUNDEF_FUNBODY(node));
        }
        current = parent_current;
        return node;
    }

    char *name = VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(node)));
    if (STReq(name, global_init_func))
    {
        // We can optimize away the __init function if it is empty.
        node_st *funbody = FUNDEF_FUNBODY(node);
        bool is_empty = (FUNBODY_VARDECS(funbody) == NULL) &&
                        (FUNBODY_LOCALFUNDEFS(funbody) == NULL) && (FUNBODY_STMTS(funbody) == NULL);
        UCset(node, is_empty ? UC_NONE : UC_USAGE);
        init_symbols = FUNDEF_SYMBOLS(node);
        FUNDEF_FUNBODY(node) = TRAVopt(FUNDEF_FUNBODY(node));

        current = parent_current;
        return node;
    }

    UCset(node, UC_USAGE);
    FUNDEF_FUNBODY(node) = TRAVopt(FUNDEF_FUNBODY(node));

    current = parent_current;
    return node;
}

node_st *OPT_DCEfunbody(node_st *node)
{
    release_assert(collect_sideeffects == false);
    release_assert(tag_usages == false);

    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    if (!remove_local_function)
    {
        // Next we collect usages of the statments
        FUNBODY_STMTS(node) = TRAVopt(FUNBODY_STMTS(node));

        // Next we remove any unused vardecs and local fundefs
        FUNBODY_VARDECS(node) = TRAVopt(FUNBODY_VARDECS(node));
    }

    FUNBODY_LOCALFUNDEFS(node) = TRAVopt(FUNBODY_LOCALFUNDEFS(node));

    return node;
}
node_st *OPT_DCEvardecs(node_st *node)
{
    release_assert(collect_sideeffects == false);
    release_assert(tag_usages == false);

    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    VARDECS_NEXT(node) = TRAVopt(VARDECS_NEXT(node));

    node_st *vardec = VARDECS_VARDEC(node);
    if (UClookup(vardec) == UC_NONE)
    {
        node_st *next = VARDECS_NEXT(node);
        VARDECS_NEXT(node) = NULL;
        node_st *var = VARDEC_VAR(vardec);
        char *name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYEXPR_VAR(var));
        node_st *entry = HTremove(current, name);
        release_assert(entry == vardec);
        CCNfree(node);
        CCNcycleNotify();
        return next;
    }

    return node;
}

node_st *OPT_DCElocalfundefs(node_st *node)
{
    // The content of local fundefs is called by the proccalls itself
    release_assert(collect_sideeffects == false);
    release_assert(tag_usages == false);

    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    LOCALFUNDEFS_NEXT(node) = TRAVopt(LOCALFUNDEFS_NEXT(node));

    release_assert(LOCALFUNDEFS_LOCALFUNDEF(node) != NULL);
    if (remove_local_function || UClookup(LOCALFUNDEFS_LOCALFUNDEF(node)) == UC_NONE)
    {
        node_st *next = LOCALFUNDEFS_NEXT(node);
        LOCALFUNDEFS_NEXT(node) = NULL;
        node_st *fundef = LOCALFUNDEFS_LOCALFUNDEF(node);
        char *name = VAR_NAME(FUNHEADER_VAR(FUNDEF_FUNHEADER(fundef)));

        bool parent_remove_local_function = remove_local_function;
        remove_local_function = true;
        // Remove all local function in this local function
        LOCALFUNDEFS_LOCALFUNDEF(node) = TRAVopt(fundef);
        remove_local_function = parent_remove_local_function;

        char *key = NULL;
        bool free_key = STRprefix("@fun_", name);
        if (free_key)
        {
            for (htable_iter_st *iter = HTiterate(current); iter; iter = HTiterateNext(iter))
            {
                key = HTiterKey(iter);
                if (STReq(key, name))
                {
                    HTiterateCancel(iter);
                    break;
                }
            }
        }

        node_st *expected = HTremove(current, name);

        if (free_key)
        {
            release_assert(key != NULL);
            free(key);
        }

        free_symbols(FUNDEF_SYMBOLS(fundef));

        release_assert(expected == fundef);
        CCNfree(node);
        CCNcycleNotify();
        return next;
    }
    else
    {
        LOCALFUNDEFS_LOCALFUNDEF(node) = TRAVopt(LOCALFUNDEFS_LOCALFUNDEF(node));
    }

    return node;
}

node_st *OPT_DCEdeclarations(node_st *node)
{
    release_assert(check_consumtion == false);
    release_assert(tag_usages == false);

    node_st *decl = DECLARATIONS_DECL(node);
    if ((NODE_TYPE(decl) == NT_FUNDEF && FUNDEF_HAS_EXPORT(decl)) ||
        (NODE_TYPE(decl) == NT_GLOBALDEF && GLOBALDEF_HAS_EXPORT(decl)))
    {
        // All decleartions and exported functions need to be checked.
        DECLARATIONS_DECL(node) = TRAVopt(decl);
    }
    DECLARATIONS_NEXT(node) = TRAVopt(DECLARATIONS_NEXT(node));

    // Every decleration stmt is the node that exists in a symbol table, execpt globaldef
    // where only the vardec is defined. Thus we located the vardec in the usage table.
    node_st *entry = NODE_TYPE(decl) == NT_GLOBALDEF ? GLOBALDEF_VARDEC(decl) : decl;
    release_assert(entry != NULL);
    if (UClookup(entry) != UC_NONE)
    {
        if (NODE_TYPE(decl) == NT_GLOBALDEF && !GLOBALDEF_HAS_EXPORT(decl))
        {
            // All none export but used globaldef need to be checked too.
            // Functions are already check when they are used by a proccall.
            DECLARATIONS_DECL(node) = TRAVopt(decl);
        }
        return node;
    }

    node_st *next = DECLARATIONS_NEXT(node);
    DECLARATIONS_NEXT(node) = NULL;

    node_st *var = NULL;
    char *name = NULL;
    bool is_extern_array = false;
    switch (NODE_TYPE(entry))
    {
    case NT_GLOBALDEC:
        var = GLOBALDEC_VAR(entry);
        name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYVAR_VAR(var));
        is_extern_array = NODE_TYPE(var) == NT_ARRAYVAR;
        break;
    case NT_VARDEC:
        var = VARDEC_VAR(entry);
        name = VAR_NAME(NODE_TYPE(var) == NT_VAR ? var : ARRAYEXPR_VAR(var));
        break;
    case NT_FUNDEF:
        var = FUNHEADER_VAR(FUNDEF_FUNHEADER(entry));
        name = VAR_NAME(var);
        bool parent_remove_local_function = remove_local_function;
        remove_local_function = true;
        // Remove all local function in this function
        DECLARATIONS_DECL(node) = TRAVopt(decl);
        remove_local_function = parent_remove_local_function;
        break;
    case NT_FUNDEC:
        var = FUNHEADER_VAR(FUNDEC_FUNHEADER(entry));
        name = VAR_NAME(var);
        break;
    default:
        release_assert(false);
        break;
    }

    char *key = NULL;
    bool free_key = STRprefix("@fun_", name);
    if (free_key)
    {
        for (htable_iter_st *iter = HTiterate(current); iter; iter = HTiterateNext(iter))
        {
            key = HTiterKey(iter);
            if (STReq(key, name))
            {
                HTiterateCancel(iter);
                break;
            }
        }
    }

    node_st *new_dec_first = NULL;
    node_st *new_dec_last = NULL;

    node_st *expected = HTremove(current, name);
    if (is_extern_array)
    {
        const char *pretty_name = get_pretty_name(name);
        node_st *dims = ARRAYVAR_DIMS(var);
        uint32_t dim_counter = 0;
        while (dims != NULL)
        {
            node_st *dim = DIMENSIONVARS_DIM(dims);
            char *name = VAR_NAME(dim);
            node_st *dim_expected = HTremove(current, name);
            release_assert(dim_expected == dims);
            if (UClookup(dims) != UC_NONE)
            {
                // We need to extract this dimension because it is used.
                // Naming should be __dim<count>_<array_name>, but we will register it under its old
                // name, to be found by a lookup
                char *new_name = STRfmt("__dim%d_%s", dim_counter++, pretty_name);
                node_st *dec = ASTglobaldec(ASTvar(new_name), DT_int, name);
                bool success = HTinsert(current, name, dec);
                release_assert(success);
                // We also insert the new name, because it is need by code_gen
                success = HTinsert(current, new_name, dec);
                release_assert(success);
                VAR_NAME(dim) = NULL;

                if (new_dec_first == NULL)
                {
                    release_assert(new_dec_last == NULL);
                    new_dec_first = ASTdeclarations(dec, next);
                    new_dec_last = new_dec_first;
                }
                else
                {
                    release_assert(new_dec_last != NULL);
                    node_st *decls = ASTdeclarations(dec, DECLARATIONS_NEXT(new_dec_last));
                    DECLARATIONS_NEXT(new_dec_last) = decls;
                    new_dec_last = decls;
                }
            }
            dims = DIMENSIONVARS_NEXT(dims);
        }
    }

    if (free_key)
    {
        release_assert(key != NULL);
        free(key);
    }

    if (NODE_TYPE(decl) == NT_FUNDEF)
    {
        free_symbols(FUNDEF_SYMBOLS(decl));
    }

    release_assert(expected == entry);
    if (NODE_TYPE(decl) == NT_GLOBALDEC && GLOBALDEC_ALIAS(decl) != NULL)
    {
        node_st *alias_expected = HTremove(current, GLOBALDEC_ALIAS(decl));
        release_assert(alias_expected == expected);
    }

    CCNfree(node);
    CCNcycleNotify();
    if (new_dec_first == NULL)
    {
        release_assert(new_dec_last == NULL);
        return next;
    }
    else
    {
        release_assert(new_dec_last != NULL);
        return new_dec_first;
    }
}

node_st *OPT_DCEstatements(node_st *node)
{
    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    if (tag_usages)
    {
        TRAVchildren(node);
        return node;
    }

    node_st *parent_sideeffect_stmts_first = sideeffect_stmts_first;
    node_st *parent_sideeffect_stmts_last = sideeffect_stmts_last;
    bool parent_collect_sideeffects = collect_sideeffects;
    sideeffect_stmts_first = NULL;
    sideeffect_stmts_last = NULL;
    collect_sideeffects = false;

    if (NODE_TYPE(STATEMENTS_STMT(node)) == NT_RETSTATEMENT)
    {
        // Add used tag to the usage stuff
        STATEMENTS_STMT(node) = TRAVopt(STATEMENTS_STMT(node));

        // Everything after the first return statmenet is never reached.
        node_st *next = STATEMENTS_NEXT(node);
        STATEMENTS_NEXT(node) = NULL;
        CCNfree(next);

        if (check_return)
        {
            has_return = true;
        }

        sideeffect_stmts_first = parent_sideeffect_stmts_first;
        sideeffect_stmts_last = parent_sideeffect_stmts_last;
        collect_sideeffects = parent_collect_sideeffects;
        return node;
    }

    STATEMENTS_NEXT(node) = TRAVopt(STATEMENTS_NEXT(node));

    node_st *stmt = STATEMENTS_STMT(node);
    node_st *entry;
    bool skip_check = false;
    bool is_local = true;       // Indicates if assigment to local variable
    bool requiere_none = false; // Indicates if assigment is an alloc call
    switch (NODE_TYPE(stmt))
    {
    case NT_ASSIGN:
        entry = deep_lookup(current, VAR_NAME(ASSIGN_VAR(stmt)));
        release_assert(entry != NULL);
        is_local = HTlookup(current, VAR_NAME(ASSIGN_VAR(stmt))) != NULL;
        requiere_none = NODE_TYPE(ASSIGN_EXPR(stmt)) == NT_PROCCALL &&
                        STReq(VAR_NAME(PROCCALL_VAR(ASSIGN_EXPR(stmt))), alloc_func);
        break;
    case NT_ARRAYASSIGN:
        entry = deep_lookup(current, VAR_NAME(ARRAYEXPR_VAR(ARRAYASSIGN_VAR(stmt))));
        release_assert(entry != NULL);
        is_local = HTlookup(current, VAR_NAME(ARRAYEXPR_VAR(ARRAYASSIGN_VAR(stmt)))) != NULL;
        requiere_none = true; // We can only optimize if the var is never used.
        break;
    case NT_IFSTATEMENT:
    case NT_WHILELOOP:
    case NT_DOWHILELOOP:
    case NT_FORLOOP:
        // Check if usage need or not
        STATEMENTS_STMT(node) = TRAVopt(STATEMENTS_STMT(node));
        entry = stmt;
        skip_check = true;
        break;
    case NT_PROCCALL:
        entry = deep_lookup(current, VAR_NAME(PROCCALL_VAR(stmt)));
        release_assert(entry != NULL);
        enum sideeffect effect = check_fun_sideeffect(entry, sideeffect_table);
        release_assert(effect != SEFF_NULL);
        release_assert(effect != SEFF_PROCESSING);
        if (effect == SEFF_YES)
        {
            // Add expression to usages and traverse the sideffecting function
            STATEMENTS_STMT(node) = TRAVopt(STATEMENTS_STMT(node));
            skip_check = true;
        }
        break;
    case NT_RETSTATEMENT:
    default:
        release_assert(false);
        break;
    }

    release_assert(current != NULL);
    bool is_init = init_symbols == current;
    release_assert(entry != NULL);
    // We need to keep sideffecting assignment as they are not used by the current function call
    // but maybe later in another function call of this sideeffecting function.
    if (UClookup(entry) != UC_USAGE && (is_local | is_init) &&
        (requiere_none ? UClookup(entry) == UC_NONE : true))
    {
        collect_sideeffects = true;
        STATEMENTS_STMT(node) = TRAVopt(STATEMENTS_STMT(node));
        collect_sideeffects = false;
        if (sideeffect_stmts_first != NULL)
        {
            release_assert(sideeffect_stmts_last != NULL);
            release_assert(STATEMENTS_NEXT(sideeffect_stmts_last) == NULL);

            // Add usage tag to all extracted sideeffecting stuff
            sideeffect_stmts_first = TRAVopt(sideeffect_stmts_first);

            STATEMENTS_NEXT(sideeffect_stmts_last) = STATEMENTS_NEXT(node);
            STATEMENTS_NEXT(node) = sideeffect_stmts_first;
        }

        // If not used we remove it.
        node_st *next = STATEMENTS_NEXT(node);
        STATEMENTS_NEXT(node) = NULL;
        CCNfree(node);
        CCNcycleNotify();

        sideeffect_stmts_first = parent_sideeffect_stmts_first;
        sideeffect_stmts_last = parent_sideeffect_stmts_last;
        collect_sideeffects = parent_collect_sideeffects;
        return next;
    }

    // Add used tag to the usage stuff
    if (!skip_check)
    {
        STATEMENTS_STMT(node) = TRAVopt(STATEMENTS_STMT(node));
    }

    sideeffect_stmts_first = parent_sideeffect_stmts_first;
    sideeffect_stmts_last = parent_sideeffect_stmts_last;
    collect_sideeffects = parent_collect_sideeffects;
    return node;
}

node_st *OPT_DCEvar(node_st *node)
{
    if (collect_sideeffects | check_consumtion)
    {
        return node;
    }

    node_st *entry = deep_lookup(current, VAR_NAME(node));
    release_assert(entry != NULL);
    UCset(entry, UC_USAGE);
    return node;
}

node_st *OPT_DCEassign(node_st *node)
{
    if (collect_sideeffects | tag_usages)
    {
        ASSIGN_EXPR(node) = TRAVopt(ASSIGN_EXPR(node));
    }
    else if (check_consumtion)
    {
        char *name = VAR_NAME(ASSIGN_VAR(node));
        node_st *entry = deep_lookup(current, name);
        release_assert(entry != NULL);
        has_consumtion = UClookup(entry) == UC_USAGE;
    }
    else
    {
        // We need to keep any assigment that produces an sideffect i.e. assign to global variable.
        char *name = VAR_NAME(ASSIGN_VAR(node));
        node_st *entry = deep_lookup(current, name);
        release_assert(entry != NULL);
        node_st *local_entry = HTlookup(current, name);
        release_assert(local_entry == NULL || UClookup(entry) == UC_USAGE ||
                       STRprefix("@for", name) ||
                       (NODE_TYPE(ASSIGN_EXPR(node)) == NT_PROCCALL &&
                        STReq(VAR_NAME(PROCCALL_VAR(ASSIGN_EXPR(node))), alloc_func)));

        if (collect_if_usages)
        {
            release_assert(if_usage_stmts != NULL);
            bool success =
                HTinsert(if_usage_stmts, entry,
                         (void *)(ptrdiff_t)((UClookup(entry) << UCshift) + UC_CONSUMED));
            release_assert(success);
        }

        if (collect_else_usages)
        {
            release_assert(else_usage_stmts != NULL);
            bool success =
                HTinsert(else_usage_stmts, entry,
                         (void *)(ptrdiff_t)((UClookup(entry) << UCshift) + UC_CONSUMED));
            release_assert(success);
        }

        UCset(entry, UC_CONSUMED);

        // collect next usages
        ASSIGN_EXPR(node) = TRAVopt(ASSIGN_EXPR(node));
    }

    return node;
}

node_st *OPT_DCEarrayassign(node_st *node)
{
    if (collect_sideeffects | tag_usages)
    {
        ARRAYASSIGN_EXPR(node) = TRAVopt(ARRAYASSIGN_EXPR(node));
        ARRAYEXPR_DIMS(ARRAYASSIGN_VAR(node)) = TRAVopt(ARRAYEXPR_DIMS(ARRAYASSIGN_VAR(node)));
    }
    else if (check_consumtion)
    {
        char *name = VAR_NAME(ARRAYEXPR_VAR(ARRAYASSIGN_VAR(node)));
        node_st *entry = deep_lookup(current, name);
        release_assert(entry != NULL);
        has_consumtion = UClookup(entry) != UC_NONE;
    }
    else
    {
        // We need to keep any assigment that produces an sideffect i.e. assign to global variable.
        char *name = VAR_NAME(ARRAYEXPR_VAR(ARRAYASSIGN_VAR(node)));
        node_st *entry = deep_lookup(current, name);
        release_assert(entry != NULL);
        node_st *local_entry = HTlookup(current, name);
        release_assert(local_entry == NULL || UClookup(entry) != UC_NONE);
        UCset(entry, UC_CONSUMED);

        if (collect_if_usages)
        {
            release_assert(if_usage_stmts != NULL);
            bool success =
                HTinsert(if_usage_stmts, entry,
                         (void *)(ptrdiff_t)((UClookup(entry) << UCshift) + UC_CONSUMED));
            release_assert(success);
        }

        if (collect_else_usages)
        {
            release_assert(else_usage_stmts != NULL);
            bool success =
                HTinsert(else_usage_stmts, entry,
                         (void *)(ptrdiff_t)((UClookup(entry) << UCshift) + UC_CONSUMED));
            release_assert(success);
        }

        // collect next usages
        ARRAYASSIGN_EXPR(node) = TRAVopt(ARRAYASSIGN_EXPR(node));
        ARRAYEXPR_DIMS(ARRAYASSIGN_VAR(node)) = TRAVopt(ARRAYEXPR_DIMS(ARRAYASSIGN_VAR(node)));
    }
    return node;
}

node_st *OPT_DCEternary(node_st *node)
{
    if (collect_sideeffects)
    {
        // We transform ternary into if statements, because we need to extract them into a
        // statement if we have sideffecting expressions.

        release_assert(TERNARY_PTRUE(node) != NULL);
        enum sideeffect effect_ptrue =
            check_expr_sideeffect(TERNARY_PTRUE(node), current, sideeffect_table);
        release_assert(effect_ptrue != SEFF_NULL);
        release_assert(effect_ptrue != SEFF_PROCESSING);

        release_assert(TERNARY_PFALSE(node) != NULL);
        enum sideeffect effect_pfalse =
            check_expr_sideeffect(TERNARY_PFALSE(node), current, sideeffect_table);
        release_assert(effect_pfalse != SEFF_NULL);
        release_assert(effect_pfalse != SEFF_PROCESSING);

        if ((effect_pfalse | effect_ptrue) == SEFF_YES)
        {
            // Transform to if statement
            node_st *convert = ASTifstatement(TERNARY_PRED(node), NULL, NULL);
            TERNARY_PRED(node) = NULL;
            if (sideeffect_stmts_first == NULL)
            {
                release_assert(sideeffect_stmts_last == NULL);

                sideeffect_stmts_first = ASTstatements(convert, NULL);
                sideeffect_stmts_last = sideeffect_stmts_first;
            }
            else
            {
                node_st *stmts = ASTstatements(convert, NULL);
                release_assert(STATEMENTS_NEXT(sideeffect_stmts_last) == NULL);
                STATEMENTS_NEXT(sideeffect_stmts_last) = stmts;
                sideeffect_stmts_last = stmts;
            }

            node_st *parent_sideeffect_stmts_first = sideeffect_stmts_first;
            node_st *parent_sideeffect_stmts_last = sideeffect_stmts_last;
            sideeffect_stmts_first = NULL;
            sideeffect_stmts_last = NULL;
            TERNARY_PTRUE(node) = TRAVopt(TERNARY_PTRUE(node));
            IFSTATEMENT_BLOCK(convert) = sideeffect_stmts_first;

            sideeffect_stmts_first = NULL;
            sideeffect_stmts_last = NULL;
            TERNARY_PFALSE(node) = TRAVopt(TERNARY_PFALSE(node));
            IFSTATEMENT_ELSE_BLOCK(convert) = sideeffect_stmts_first;

            CCNfree(node);
            CCNcycleNotify();
            sideeffect_stmts_first = parent_sideeffect_stmts_first;
            sideeffect_stmts_last = parent_sideeffect_stmts_last;
            return NULL;
        }
    }
    else if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    TRAVchildren(node);
    return node;
}

node_st *OPT_DCEproccall(node_st *node)
{
    char *name = VAR_NAME(PROCCALL_VAR(node));

    if (STReq(name, alloc_func))
    {
        // Usages are collected through VAR
        PROCCALL_EXPRS(node) = TRAVopt(PROCCALL_EXPRS(node));
        return node;
    }

    node_st *entry = deep_lookup(current, name);
    release_assert(entry != NULL);
    if (collect_sideeffects)
    {
        enum sideeffect effect = check_fun_sideeffect(entry, sideeffect_table);
        release_assert(effect != SEFF_NULL);
        release_assert(effect != SEFF_PROCESSING);

        if (effect == SEFF_YES)
        {
            if (sideeffect_stmts_first == NULL)
            {
                release_assert(sideeffect_stmts_last == NULL);

                sideeffect_stmts_first = ASTstatements(node, NULL);
                sideeffect_stmts_last = sideeffect_stmts_first;
            }
            else
            {
                node_st *stmts = ASTstatements(node, NULL);
                release_assert(STATEMENTS_NEXT(sideeffect_stmts_last) == NULL);
                STATEMENTS_NEXT(sideeffect_stmts_last) = stmts;
                sideeffect_stmts_last = stmts;
            }

            // dead code optimized any sideeffecting extracted proccall.
            collect_sideeffects = false;
            bool already_optimized = UClookup(entry) != UC_USAGE;
            UCset(entry, UC_USAGE);
            PROCCALL_EXPRS(node) = TRAVopt(PROCCALL_EXPRS(node));
            if (NODE_TYPE(entry) == NT_FUNDEF && already_optimized)
            {
                TRAVopt(entry);
            }
            collect_sideeffects = true;

            return NULL;
        }
        else
        {
            // Get sideffect expression from proccall parameter expressions
            PROCCALL_EXPRS(node) = TRAVopt(PROCCALL_EXPRS(node));
        }
    }
    else if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVopt(PROCCALL_EXPRS(node));
        }
    }
    else if (tag_usages)
    {
        TRAVopt(PROCCALL_EXPRS(node));
    }
    else
    {
        bool already_optimized = UClookup(entry) != UC_USAGE;
        UCset(entry, UC_USAGE);
        // Usages are collected through VAR
        PROCCALL_EXPRS(node) = TRAVopt(PROCCALL_EXPRS(node));
        // We also need to check the content of the next function as it might be local an set usage
        // of any upper function variable.
        if (NODE_TYPE(entry) == NT_FUNDEF && already_optimized)
        {
            TRAVopt(entry);
        }
    }
    return node;
}

node_st *OPT_DCEdowhileloop(node_st *node)
{
    if (collect_sideeffects)
    {
        // This node will not be removed if we have any sideeffects, because the expression
        // in the loop is called an unkown number of times and can be set in a stopping state
        // by a proccall to a local function. Thus we do not need to collect anything from here.
        return node;
    }

    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    if (tag_usages)
    {
        TRAVchildren(node);
        return node;
    }

    // collect usage of the expression in the check as we go bottom up during usage collection
    if (check_stmts_sideffect(DOWHILELOOP_BLOCK(node), current) ||
        has_consume(DOWHILELOOP_BLOCK(node)))
    {
        DOWHILELOOP_EXPR(node) = TRAVopt(DOWHILELOOP_EXPR(node));
        tag_usages = true;
        DOWHILELOOP_BLOCK(node) = TRAVopt(DOWHILELOOP_BLOCK(node));
        tag_usages = false;
    }

    bool parent_check_return = check_return;
    bool parent_has_return = has_return;
    check_return = true;
    has_return = false;
    DOWHILELOOP_BLOCK(node) = TRAVopt(DOWHILELOOP_BLOCK(node)); // Statments may change
    if (has_return)
    {
        // We never reach the expression, thus we can remove it
        CCNfree(DOWHILELOOP_EXPR(node));
        DOWHILELOOP_EXPR(node) = ASTbool(false); // Optimize out in outer pass
        CCNcycleNotify();
    }
    check_return = parent_check_return;
    has_return = parent_has_return;

    if (DOWHILELOOP_BLOCK(node) == NULL)
    {
        release_assert(DOWHILELOOP_EXPR(node) != NULL);
        enum sideeffect effect =
            check_expr_sideeffect(DOWHILELOOP_EXPR(node), current, sideeffect_table);
        release_assert(effect != SEFF_NULL);
        release_assert(effect != SEFF_PROCESSING);
        if (effect == SEFF_NO)
        {
            // We can remove the dowhile loop completely. Thus we dont add it to the usage table.
            return node;
        }
    }

    // We do not remove and have to mark any in the expression as usage
    DOWHILELOOP_EXPR(node) = TRAVopt(DOWHILELOOP_EXPR(node));
    UCset(node, UC_USAGE);
    return node;
}

node_st *OPT_DCEforloop(node_st *node)
{
    if (collect_sideeffects)
    {
        // This node will not be removed if we have any sideeffects, because the expression
        // in the loop is called an n times and we need to execute the sideeffects too.
        // Thus we do not need to collect anything from here.
        return node;
    }

    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    if (tag_usages)
    {
        TRAVchildren(node);
        return node;
    }

    bool is_consuming =
        check_stmts_sideffect(FORLOOP_BLOCK(node), current) || has_consume(FORLOOP_BLOCK(node));
    htable_stptr parent_if_usage_stmts = if_usage_stmts;
    htable_stptr parent_else_usage_stmts = else_usage_stmts;
    bool parent_collect_if_usages = collect_if_usages;
    bool parent_collect_else_usages = collect_else_usages;
    if_usage_stmts = HTnew_Ptr(2 << 4);
    else_usage_stmts = HTnew_Ptr(2 << 4);

    collect_else_usages = false;
    collect_if_usages = true;
    if (is_consuming)
    {
        tag_usages = true;
        FORLOOP_BLOCK(node) = TRAVopt(FORLOOP_BLOCK(node));
        tag_usages = false;
    }
    FORLOOP_BLOCK(node) = TRAVopt(FORLOOP_BLOCK(node));
    collect_if_usages = false;
    UCrestore();

    HTdelete(if_usage_stmts);
    HTdelete(else_usage_stmts);
    if_usage_stmts = parent_if_usage_stmts;
    else_usage_stmts = parent_else_usage_stmts;
    collect_if_usages = parent_collect_if_usages;
    collect_else_usages = parent_collect_else_usages;

    if (FORLOOP_BLOCK(node) == NULL)
    {
        // if any condition does not have sideeffects we can remove it.
        enum sideeffect effect0 =
            FORLOOP_ITER(node) == NULL
                ? SEFF_NO
                : check_expr_sideeffect(FORLOOP_ITER(node), current, sideeffect_table);
        release_assert(effect0 != SEFF_NULL);
        release_assert(effect0 != SEFF_PROCESSING);

        release_assert(FORLOOP_COND(node) != NULL);
        enum sideeffect effect1 =
            check_expr_sideeffect(FORLOOP_COND(node), current, sideeffect_table);
        release_assert(effect1 != SEFF_NULL);
        release_assert(effect1 != SEFF_PROCESSING);

        release_assert(FORLOOP_ASSIGN(node) != NULL);
        enum sideeffect effect2 =
            check_stmt_sideeffect(FORLOOP_ASSIGN(node), current, sideeffect_table);
        release_assert(effect2 != SEFF_NULL);
        release_assert(effect2 != SEFF_PROCESSING);

        if ((effect0 | effect1 | effect2) == SEFF_NO)
        {
            // We can remove by no adding it to the usage table.
            return node;
        }
    }

    FORLOOP_ITER(node) = TRAVopt(FORLOOP_ITER(node));
    FORLOOP_COND(node) = TRAVopt(FORLOOP_COND(node));
    FORLOOP_ASSIGN(node) = TRAVopt(FORLOOP_ASSIGN(node));
    UCset(node, UC_USAGE);
    return node;
}

node_st *OPT_DCEwhileloop(node_st *node)
{
    if (collect_sideeffects)
    {
        // This node will not be removed if we have any sideeffects, because the expression
        // in the loop is called an unkown number of times and can be set in a stopping state
        // by a proccall to a local function. Thus we do not need to collect anything from here.
        return node;
    }

    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    if (tag_usages)
    {
        TRAVchildren(node);
        return node;
    }

    // Optimize by not calling, if no other than the here check expression is consumed
    // in the while loop and no sideffect, i.e. we can optimize the loop away with assignment to the
    // while expr. NOTE: we can do this for the do while loop too.
    bool is_consumed =
        check_stmts_sideffect(WHILELOOP_BLOCK(node), current) || has_consume(WHILELOOP_BLOCK(node));
    if (is_consumed)
    {
        // The block may change anything in the expression
        WHILELOOP_EXPR(node) = TRAVopt(WHILELOOP_EXPR(node));
    }

    htable_stptr parent_if_usage_stmts = if_usage_stmts;
    htable_stptr parent_else_usage_stmts = else_usage_stmts;
    bool parent_collect_if_usages = collect_if_usages;
    bool parent_collect_else_usages = collect_else_usages;
    if_usage_stmts = HTnew_Ptr(2 << 4);
    else_usage_stmts = HTnew_Ptr(2 << 4);

    collect_if_usages = true;
    collect_else_usages = false;
    if (is_consumed)
    {
        tag_usages = true;
        WHILELOOP_BLOCK(node) = TRAVopt(WHILELOOP_BLOCK(node));
        tag_usages = false;
    }
    WHILELOOP_BLOCK(node) = TRAVopt(WHILELOOP_BLOCK(node)); // Statments may change
    collect_if_usages = false;
    UCrestore();

    HTdelete(if_usage_stmts);
    HTdelete(else_usage_stmts);
    if_usage_stmts = parent_if_usage_stmts;
    else_usage_stmts = parent_else_usage_stmts;
    collect_if_usages = parent_collect_if_usages;
    collect_else_usages = parent_collect_else_usages;

    if (WHILELOOP_BLOCK(node) == NULL)
    {
        release_assert(WHILELOOP_EXPR(node) != NULL);
        enum sideeffect effect =
            check_expr_sideeffect(WHILELOOP_EXPR(node), current, sideeffect_table);
        release_assert(effect != SEFF_NULL);
        release_assert(effect != SEFF_PROCESSING);
        if (effect == SEFF_NO)
        {
            // We can remove the while loop completely. Thus we dont add it to the usage table.
            return node;
        }
    }

    WHILELOOP_EXPR(node) = TRAVopt(WHILELOOP_EXPR(node));
    UCset(node, UC_USAGE);
    return node;
}

node_st *OPT_DCEifstatement(node_st *node)
{
    if (collect_sideeffects)
    {
        // collect side effecting expression. These are evaluated only once so we are allowed to do
        // this.
        IFSTATEMENT_EXPR(node) = TRAVopt(IFSTATEMENT_EXPR(node));
        return node;
    }

    if (check_consumtion)
    {
        if (!has_consumtion)
        {
            TRAVchildren(node);
        }
        return node;
    }

    if (tag_usages)
    {
        TRAVchildren(node);
        return node;
    }

    htable_stptr parent_if_usage_stmts = if_usage_stmts;
    htable_stptr parent_else_usage_stmts = else_usage_stmts;
    bool parent_collect_if_usages = collect_if_usages;
    bool parent_collect_else_usages = collect_else_usages;
    if_usage_stmts = HTnew_Ptr(2 << 4);
    else_usage_stmts = HTnew_Ptr(2 << 4);

    collect_else_usages = false;
    collect_if_usages = true;
    IFSTATEMENT_BLOCK(node) = TRAVopt(IFSTATEMENT_BLOCK(node));
    UCrestore();

    collect_if_usages = false;
    collect_else_usages = true;
    IFSTATEMENT_ELSE_BLOCK(node) = TRAVopt(IFSTATEMENT_ELSE_BLOCK(node));
    collect_else_usages = false;
    UCrestore();

    HTdelete(if_usage_stmts);
    HTdelete(else_usage_stmts);
    if_usage_stmts = parent_if_usage_stmts;
    else_usage_stmts = parent_else_usage_stmts;
    collect_if_usages = parent_collect_if_usages;
    collect_else_usages = parent_collect_else_usages;

    if (IFSTATEMENT_BLOCK(node) == NULL && IFSTATEMENT_ELSE_BLOCK(node) == NULL)
    {
        // We can remove the if statement completely. Thus we dont add it to the usage table.
        // Note: side effecting expression are collected and written as additional statements.
        return node;
    }

    IFSTATEMENT_EXPR(node) = TRAVopt(IFSTATEMENT_EXPR(node));
    UCset(node, UC_USAGE);
    return node;
}
