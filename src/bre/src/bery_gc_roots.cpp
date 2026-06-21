#include "../include/bery_gc_roots.h"
#include "../include/bery_runtime.h"
#include <cstdlib>

void bery_gc_push_root(void** slot) {
    Root* root = static_cast<Root*>(malloc(sizeof(Root)));
    root->slot = slot;
    root->next = static_cast<Root*>(g_beryRuntime.rootStackHead);
    g_beryRuntime.rootStackHead = root;
}

void bery_gc_pop_root() {
    Root* head = static_cast<Root*>(g_beryRuntime.rootStackHead);

    if (!head) return;

    g_beryRuntime.rootStackHead = head->next;
    free(head);
}

Root* bery_gc_roots_head() {
    return static_cast<Root*>(g_beryRuntime.rootStackHead);
}