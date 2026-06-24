#include "../include/bery_native.h"
#include "../include/bery_string.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static char g_outputBuffer[BERY_OUTPUT_BUFFER_SIZE];
static size_t g_outputPos = 0;


static void bufferWrite(const char* data, size_t length) {
    size_t remaining = length;
    const char* src = data;

    while (remaining > 0) {
        size_t space = BERY_OUTPUT_BUFFER_SIZE - g_outputPos;
        size_t chunk = remaining < space ? remaining : space;

        memcpy(g_outputBuffer + g_outputPos, src, chunk);
        g_outputPos += chunk;
        remaining -= chunk;
        src += chunk;

        if (g_outputPos == BERY_OUTPUT_BUFFER_SIZE) {
            bery_output_flush();

        }
    }
}

static void bufferWriteStr(const char* cstr) {
    bufferWrite(cstr, strlen(cstr));
}
void bery_output_flush() {
    if (g_outputPos == 0) return;
    fwrite(g_outputBuffer, 1, g_outputPos, stdout);
    fflush(stdout);
    g_outputPos = 0;
}

// @these are all prining statement, we first infer type and route to one fo these functions.

void bery_print_int(int64_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)value);
    bufferWriteStr(buf);
}
void bery_print_bigint(int64_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)value);
    bufferWriteStr(buf);
}
void bery_print_float(float value){
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6f", value);
    bufferWriteStr(buf);
}
void bery_print_double(double value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6f", value);
    bufferWriteStr(buf);
}
void bery_print_bool(bool value){bufferWriteStr(value ? "true" : "false");}


void bery_print_char(char value) {bufferWrite(&value, 1);}

void bery_print_string(BeryString* value){
    if (!value) return;
    bufferWrite(value->data, value->length);
}

void bery_println() {
    bufferWrite("\n", 1);
}

// @these are all input functions different for different datatype : 


int32_t bery_input_int(const char* prompt) {
    if (prompt) { bufferWriteStr(prompt); bery_output_flush(); }
    int32_t val = 0;
    scanf("%d", &val);
    return val;
}

int64_t bery_input_bigint(const char* prompt) {
    if (prompt) { bufferWriteStr(prompt); bery_output_flush(); }
    int64_t val = 0;
    scanf("%lld", (long long*)&val);
    return val;
}

float bery_input_float(const char* prompt) {
    if (prompt) { bufferWriteStr(prompt); bery_output_flush(); }
    float val = 0.0f;
    scanf("%f", &val);
    return val;
}

double bery_input_double(const char* prompt) {
    if (prompt) { bufferWriteStr(prompt); bery_output_flush(); }
    double val = 0.0;
    scanf("%lf", &val);
    return val;
}

bool bery_input_bool(const char* prompt) {
    if (prompt) { bufferWriteStr(prompt); bery_output_flush(); }
    int val = 0;
    scanf("%d", &val);
    return val != 0;
}

char bery_input_char(const char* prompt) {
    if (prompt) { bufferWriteStr(prompt); bery_output_flush(); }
    char val = 0;
    scanf(" %c", &val);
    return val;
}


void __bery_print_int(int32_t v)        { 
    bery_print_int(v); 
}
void __bery_print_bigint(int64_t v)     { 
    bery_print_bigint(v); 
}
void __bery_print_float(float v)        { 
    bery_print_float(v); 
}
void __bery_print_double(double v)      {
     bery_print_double(v); 
}
void __bery_print_bool(bool v)          { 
    bery_print_bool(v); 
}
void __bery_print_char(char v)          { 
    bery_print_char(v); 
}
void __bery_print_string(const char* v) {
    if (!v) return;
    bufferWriteStr(v);
}

int32_t  __bery_input_int(const char* p)    { 
    return bery_input_int(p);
}
int64_t  __bery_input_bigint(const char* p) { 
    return bery_input_bigint(p);
}
float    __bery_input_float(const char* p)  { 
    return bery_input_float(p); 
}
double   __bery_input_double(const char* p) { 
    return bery_input_double(p); 
}
bool     __bery_input_bool(const char* p)   { 
    return bery_input_bool(p); 
}
char     __bery_input_char(const char* p)   { 
    return bery_input_char(p); 
}