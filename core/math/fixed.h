#pragma once

#include <stdint.h>

static const int fixed_k = 1000;
typedef int64_t fixed;
typedef uint64_t ufixed;

#define FIXED_ENCODE(x) ((x) * fixed_k)
#define FIXED_DECODE(x) ((x) / fixed_k)