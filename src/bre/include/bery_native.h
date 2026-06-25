#pragma once
#include "bery_string.h"
#include <cstdint>

constexpr size_t BERY_OUTPUT_BUFFER_SIZE = 4096;

extern "C" {
    void bery_output_flush();

    void bery_print_int(int64_t value);
    void bery_print_bigint(int64_t value);
    void bery_print_float(float value);
    void bery_print_double(double value);
    void bery_print_bool(bool value);
    void bery_print_char(char value);
    void bery_print_string(BeryString* value);
    void bery_println();

    BeryString* bery_input(const char* prompt);
    int32_t  bery_input_int(const char* prompt);
    int64_t  bery_input_bigint(const char* prompt);
    float    bery_input_float(const char* prompt);
    double   bery_input_double(const char* prompt);
    bool     bery_input_bool(const char* prompt);
    char     bery_input_char(const char* prompt);

    void __bery_print_int(int32_t v);
    void __bery_print_bigint(int64_t v);
    void __bery_print_float(float v);
    void __bery_print_double(double v);
    void __bery_print_bool(bool v);
    void __bery_print_char(char v);
    void __bery_print_string(BeryString* v);
    void __bery_print_cstr(const char* v);
    

    int32_t  __bery_input_int(const char* p);
    int64_t  __bery_input_bigint(const char* p);
    float    __bery_input_float(const char* p);
    double   __bery_input_double(const char* p);
    bool     __bery_input_bool(const char* p);
    char     __bery_input_char(const char* p);
}