
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav.h"
#include "ccngen/trav_data.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include <assert.h>
#include <stdio.h>

void increment_name(htable_st *htable, char *name) {
    assert(htable != NULL);
    void *entry = HTlookup(htable, name);
    if (entry != NULL) {
        HTinsert(htable, name, (void *)((unsigned int)entry + 1));
    } else {
        HTinsert(htable, name, (void *)1);
    }
}

void CIDinit() { DATA_CID_GET()->count_ids = HTnew_String(2 << 12); } // 8192
void CIDfini() {
    htable_st *htable = DATA_CID_GET()->count_ids;
    if (htable != NULL) {
        HTdelete(htable);
    }
}

node_st *CIDprogram(node_st *node) {
    TRAVchildren(node);

    htable_st *htable = DATA_CID_GET()->count_ids;
    assert(htable != NULL);
    // Print identifiers count
    for (htable_iter_st *iter = HTiterate(htable); iter; iter = HTiterateNext(iter)) {
        // Getter functions to extract htable elements
        void *key = HTiterKey(iter);
        void *value = HTiterValue(iter);

        printf("%s: %d\n", (char *)key, (unsigned int)value);
    }
    return node;
}

node_st *CIDvar(node_st *node) {
    htable_st *htable = DATA_CID_GET()->count_ids;
    assert(htable != NULL);
    increment_name(htable, VAR_NAME(node));
    return node;
}

node_st *CIDvarlet(node_st *node) {
    htable_st *htable = DATA_CID_GET()->count_ids;
    assert(htable != NULL);
    increment_name(htable, VARLET_NAME(node));
    return node;
}
