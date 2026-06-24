#include "../include/bery_runtime.h"
#include "../include/bery_string.h"
#include "../include/bery_array.h"
#include "../include/bery_type_registry.h"
#include "../include/bery_native.h"
#include <cstdlib>

BeryRuntimeState g_beryRuntime;

void bery_runtime_startup() {
    g_beryRuntime.heapHead = nullptr;
    g_beryRuntime.rootStackHead = nullptr;

    g_beryRuntime.typeRegistry = nullptr;
    g_beryRuntime.totalAllocated = 0;
    g_beryRuntime.allocationCount = 0;
    g_beryRuntime.totalObjectsLive = 0;

    bery_string_init_type();
    bery_array_init_type();
}

void bery_runtime_shutdown() {
    bery_output_flush();

    BeryObjectHeader* current = g_beryRuntime.heapHead;
    while (current) {
        BeryObjectHeader* next = current->next;

        BeryTypeInfo* type = bery_type_lookup(current->typeId);
        if (type && type->destructor) {
            void* payload = reinterpret_cast<char*>(current) + sizeof(BeryObjectHeader);
            type->destructor(payload);
        }

        free(current);
        current = next;
    }

    g_beryRuntime.heapHead = nullptr;
    g_beryRuntime.totalAllocated  = 0;
    g_beryRuntime.totalObjectsLive = 0;


     bery_type_registry_shutdown();
}