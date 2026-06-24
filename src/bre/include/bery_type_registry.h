#pragma once
#include <cstdint>
#include <cstddef>

struct BeryFieldInfo {
    size_t offset;
};

typedef void (*BeryDestructorFn)(void* payload);

struct BeryTypeInfo {
    uint32_t typeId;
    const char* typeName;
    size_t instanceSize;
    BeryFieldInfo* pointerFields;
    size_t pointerFieldCount;
    BeryDestructorFn destructor;
};

extern "C" {
    uint32_t bery_type_register(const char* typeName, size_t instanceSize, BeryFieldInfo* pFields, size_t pfCount,BeryDestructorFn destructor);
    BeryTypeInfo* bery_type_lookup(uint32_t typeId);
    void bery_type_registry_shutdown();
}
