#pragma once
#include <cstddef>

constexpr unsigned int BERY_GC_ALLOC_THRESHHOLD = 1000;
constexpr size_t BERY_GC_HEAP_SIZE_THRESHHOLD = 4 * 1024 * 1024;


extern "C" {
    void bery_gc_collect();
}

bool bery_gc_should_collect();