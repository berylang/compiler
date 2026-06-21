#include "../include/bery_runtime.h"
#include <cstdlib>

BeryRuntimeState g_beryRuntime;

void bery_runtime_startup() {
    g_beryRuntime.heapHead = nullptr;
    g_beryRuntime.rootStackHead = nullptr;

    g_beryRuntime.typeRegistry = nullptr;
    g_beryRuntime.totalAllocated = 0;
    g_beryRuntime.allocationCount = 0;
    g_beryRuntime.totalObjectsLive = 0;
}

void bery_rutnime_shutdown() {
    BeryObjectHeader* current = g_beryRuntime.heapHead;
    while (current) {
        BeryObjectHeader* next = current->next;
        free(current);
        current = next;
    }

    g_beryRuntime.heapHead = nullptr;
    g_beryRuntime.totalAllocated = 0;
    g_beryRuntime.totalObjectsLive = 0;

    // @todo: free typeRegistery after implementing it later.
}