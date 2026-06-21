#pragma once
#include "bery_object.h"
#include <cstddef>

struct BeryRuntimeState {
    BeryObjectHeader* heapHead;
    void* rootStackHead;
    void* typeRegistry;

    size_t totalAllocated;
    size_t allocationCount;
    size_t totalObjectsLive;
};

extern BeryRuntimeState g_beryRuntime;

extern "C" {
    void bery_runtime_startup();
    void bery_rutnime_shutdown();
}
