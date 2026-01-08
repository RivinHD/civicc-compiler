#include "ccngen/ast.h"
#include "palm/str.h"
#include "release_assert.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static node_st *funbody = NULL;
static node_st *last_vardecs = NULL;
static uint32_t for_counter = 0;
static char *old_for_name = NULL;
static char *new_for_name = NULL;

node_st *CA_EFIfundef(node_st *node)
{

    uint32_t parent_for_counter = for_counter;
    node_st *parent_last_vardecs = last_vardecs;
    node_st *parent_funbody = funbody;
    last_vardecs = NULL;
    for_counter = 0;
    funbody = FUNDEF_FUNBODY(node);
    TRAVopt(FUNDEF_FUNBODY(node));
    last_vardecs = parent_last_vardecs;
    for_counter = parent_for_counter;
    funbody = parent_funbody;

    return node;
}

node_st *CA_EFIvardecs(node_st *node)
{
    if (VARDECS_NEXT(node) == NULL)
    {
        last_vardecs = node;
    }

    TRAVopt(VARDECS_NEXT(node));
    return node;
}

node_st *CA_EFIforloop(node_st *node)
{
    release_assert(funbody != NULL);
    if (last_vardecs != NULL)
    {
        release_assert(VARDECS_NEXT(last_vardecs) == NULL);
    }

    // Replace potential outer for loop variables
    TRAVopt(FORLOOP_COND(node));
    TRAVopt(FORLOOP_ITER(node));
    node_st *assign = FORLOOP_ASSIGN(node);
    TRAVopt(ASSIGN_EXPR(assign));

    char *old_name = VAR_NAME(ASSIGN_VAR(assign));
    if (STRprefix("@for", old_name))
    {
        // already renamed, Replace potential out for loop variables
        TRAVopt(FORLOOP_BLOCK(node));
        return node;
    }

    char *new_name = STRfmt("@for%d_%s", for_counter++, old_name);
    VAR_NAME(ASSIGN_VAR(assign)) = new_name;
    node_st *vardec = ASTvardec(CCNcopy(ASSIGN_VAR(assign)), NULL, DT_int);
    node_st *vardecs = ASTvardecs(vardec, NULL);
    if (last_vardecs != NULL)
    {
        VARDECS_NEXT(last_vardecs) = vardecs;
    }
    else
    {
        FUNBODY_VARDECS(funbody) = vardecs;
    }
    last_vardecs = vardecs;

    // Replace all variables inside the block, but ensure the shadowing
    old_for_name = old_name;
    new_for_name = new_name;
    TRAVopt(FORLOOP_BLOCK(node));
    old_for_name = NULL;
    new_for_name = NULL;

    // Run again to replace inner for loop blocks that are now already renamed with @for
    old_for_name = old_name;
    new_for_name = new_name;
    TRAVopt(FORLOOP_BLOCK(node));
    old_for_name = NULL;
    new_for_name = NULL;

    free(old_name);
    return node;
}

node_st *CA_EFIvar(node_st *node)
{
    // Potential replace this for name if inside a for block
    if (old_for_name != NULL || new_for_name != NULL)
    {
        release_assert(old_for_name != NULL);
        release_assert(new_for_name != NULL);

        char *name = VAR_NAME(node);
        if (STReq(name, old_for_name))
        {
            VAR_NAME(node) = STRcpy(new_for_name);
            free(name);
        }
    }
    return node;
}
