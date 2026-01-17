/**
 * @file
 *
 * This file contains the code for the IdentifierCounting traversal.
 * The traversal has the uid: BC
 *
 */

#include <stdio.h>

#include "palm/hash_table.h"
#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "ccngen/trav.h"
#include "ccngen/trav_data.h"

void ICinit() { return; }
void ICfini() { return; }

node_st *ICvar( node_st *node )
{
    htable_st *table = 

    return node;
}
// node_st *ICvar( node_st *node )
// {
//     struct data_bc *data = DATA_IC_GET();

//     switch ( BINOP_OP( node ) )
//     {
//         case BO_add:
//             data->count_add += 1;
//             data->count_all += 1;
//             break;
//         case BO_sub:
//             data->count_sub += 1;
//             data->count_all += 1;
//             break;
//         case BO_mul:
//             data->count_mul += 1;
//             data->count_all += 1;
//             break;
//         case BO_div:
//             data->count_div += 1;
//             data->count_all += 1;
//             break;
//         case BO_mod:
//             data->count_mod += 1;
//             data->count_all += 1;
//             break;
//         default:
//             break;
//     }

//     TRAVleft( node );
//     TRAVright( node );
//     return node;
// }

// node_st *BCprogramroot( node_st *node )
// {
//     struct data_bc *data = DATA_BC_GET();

//     TRAVchildren( node );

//     for ( int i = 0; i < )
//     PROGRAMROOT_COUNT_ADD( node ) = data->count_add;
//     PROGRAMROOT_COUNT_SUB( node ) = data->count_sub;
//     PROGRAMROOT_COUNT_MUL( node ) = data->count_mul;
//     PROGRAMROOT_COUNT_DIV( node ) = data->count_div;
//     PROGRAMROOT_COUNT_MOD( node ) = data->count_mod;
//     PROGRAMROOT_COUNT_ALL( node ) = data->count_all;

//     return node;
// }


