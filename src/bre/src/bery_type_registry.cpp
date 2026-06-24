#include "../include/bery_type_registry.h"
#include "../include/bery_runtime.h"
#include <cstdlib>
#include <cstring>

struct BeryTypeRegistry {
    BeryTypeInfo* types;
    uint32_t count;
    uint32_t capacity;
};

static BeryTypeRegistry* registry() {
    if (!g_beryRuntime.typeRegistry) {
        BeryTypeRegistry* reg = static_cast<BeryTypeRegistry*>(malloc(sizeof(BeryTypeRegistry)));

        reg->capacity = 8;
        reg->count = 0;
        reg->types = static_cast<BeryTypeInfo*>(malloc(sizeof(BeryTypeInfo) * reg->capacity));
        g_beryRuntime.typeRegistry = reg;
    }
    return static_cast<BeryTypeRegistry*>(g_beryRuntime.typeRegistry);
}

uint32_t bery_type_register(const char* typeName, size_t instanceSize, BeryFieldInfo* pFields, size_t pfCount, BeryDestructorFn destructor) {
    BeryTypeRegistry* reg = registry();

    if (reg->count == reg->capacity) {
        reg->capacity *= 2;
        reg->types = static_cast<BeryTypeInfo*>(realloc(reg->types, sizeof(BeryTypeInfo) * reg->capacity));
    }

    uint32_t newId = reg->count;
    BeryTypeInfo* info = &reg->types[newId];
    info->typeId = newId;
    info->typeName = typeName;
    info->instanceSize = instanceSize;
    info->pointerFields = pFields;
    info->pointerFieldCount = pfCount;
    info->destructor = destructor;

    reg->count += 1;
    return newId;
}

BeryTypeInfo* bery_type_lookup(uint32_t typeId) {
    BeryTypeRegistry* reg = registry();
    if (typeId >= reg->count) return nullptr;
    return &reg->types[typeId];
}
void bery_type_registry_shutdown() {
    if (!g_beryRuntime.typeRegistry) return;
    BeryTypeRegistry* reg = static_cast<BeryTypeRegistry*>(g_beryRuntime.typeRegistry);
    free(reg->types);
    free(reg);
    g_beryRuntime.typeRegistry = nullptr;
}