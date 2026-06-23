#include "../include/bery_array.h"
#include "../include/bery_alloc.h"
#include "../include/bery_type_registry.h"
#include "../include/bery_object.h"
#include <cstring>
#include <cstdlib>

uint32_t g_beryArrayTypeId = 0;

static BeryFieldInfo g_arrayPointerFields[1] = {
    { offsetof(BeryArray, data) }
};

void bery_array_init_type() {
    g_beryArrayTypeId = bery_type_register("array", sizeof(BeryArray), g_arrayPointerFields, 1, nullptr);
}

static void* allocateDataBuffer(size_t capacity) {
    void* buffer = bery_alloc(sizeof(void*) * capacity, g_beryArrayTypeId);
    memset(buffer, 0, sizeof(void*) * capacity);
    return buffer;
}

BeryArray* bery_array_new(size_t initialCapacity) {
    if (initialCapacity == 0) initialCapacity = 4;
    BeryArray* arr = static_cast<BeryArray*>(bery_alloc(sizeof(BeryArray), g_beryArrayTypeId));
    arr->length = 0;
    arr->capacity = initialCapacity;
    arr->data = static_cast<void**>(allocateDataBuffer(initialCapacity));

    return arr;
}

static void growIfNeeded(BeryArray* arr) {
    if (arr->length < arr->capacity) return;

    size_t newCapacity = arr->capacity * 2;
    void** newData = static_cast<void**>(allocateDataBuffer(newCapacity));
    memcpy(newData, arr->data, sizeof(void*) * arr->length);
    arr->data = newData;
    arr->capacity = newCapacity;
}

void bery_array_push(BeryArray* arr, void* element) {
    growIfNeeded(arr);
    arr->data[arr->length] = element;
    arr->length += 1;


}

void* bery_array_pop(BeryArray* arr) {
    if (arr->length == 0) return nullptr;
    arr->length -= 1;

    return arr->data[arr->length];
}

void bery_array_insert(BeryArray* arr, size_t index, void* element) {
    growIfNeeded(arr);
    for (size_t i = arr->length; i > index; i--) {arr->data[i] = arr->data[i - 1];}
    arr->data[index] = element;
    arr->length += 1;
}

void bery_array_remove(BeryArray* arr, size_t index) {
    if (index >= arr->length) return;
    for (size_t i = index; i < arr->length - 1; i++) {

        arr->data[i] = arr->data[i + 1];

    }
    arr->length -= 1;
}

void* bery_array_get(BeryArray* arr, size_t index)  {
    if (index >= arr->length) return nullptr;
    return arr->data[index];
}

size_t bery_array_length(BeryArray* arr)  {
    return arr->length;
}