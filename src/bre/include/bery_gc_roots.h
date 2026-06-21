#pragma once

// @shadow_stack
struct Root {
    void** slot;
    Root* next;
};

extern "C" {
    void bery_gc_push_root(void** slot);
    void bery_gc_pop_root();
}

Root* bery_gc_roots_head();