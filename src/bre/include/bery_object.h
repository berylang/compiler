#pragma once
#include <cstdint>
#include <cstddef>

enum class BeryBuiltInTypeId : uint32_t {
    STRING = 0, 
    ARRAY = 1,
    OBJECT = 2
};

struct BeryObjectHeader {
    uint8_t marked;
    uint32_t typeId;
    size_t size;
    BeryObjectHeader* next;
};