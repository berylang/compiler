#pragma once
#include <cstdint>
#include <cstddef>

struct BeryArray {
    size_t length;
    size_t capacity;
    void** data;
};

extern "C" {
    void bery_array_init_type();

    BeryArray* bery_array_new(size_t initialCapacity);
    void bery_array_push(BeryArray* arr, void* element);
    void* bery_array_pop(BeryArray* arr);
    void bery_array_insert(BeryArray* arr, size_t index, void* element);
    void bery_array_remove(BeryArray* arr, size_t index);
    void* bery_array_get(BeryArray* arr, size_t index);
    size_t bery_array_length(BeryArray* arr);
}

extern uint32_t g_beryArrayTypeId;