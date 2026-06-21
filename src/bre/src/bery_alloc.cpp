#include <cstdlib>
#include "../include/bery_alloc.h"
#include "../include/bery_runtime.h"



void* bery_alloc(size_t size, uint32_t typeId) {
    size_t totalSize = sizeof(BeryObjectHeader) + size;
    BeryObjectHeader* header  = static_cast<BeryObjectHeader*>(malloc(totalSize));
    
    header->marked = 0;
    header->typeId = typeId;
    header->size   = size;
    header->next = g_beryRuntime.heapHead;
    g_beryRuntime.heapHead = header;

    g_beryRuntime.totalAllocated += size;
    g_beryRuntime.allocationCount += 1;
    g_beryRuntime.totalObjectsLive += 1;

    return reinterpret_cast<char*>(header) + sizeof(BeryObjectHeader);
}

BeryObjectHeader* bery_header_from_payload(void* payload) {
    return reinterpret_cast<BeryObjectHeader*>(reinterpret_cast<char*>(payload) - sizeof(BeryObjectHeader));
}