#pragma once
#include "RuntimeAlias.h"

#define CPU_FEATURE_AVX
#define CPU_FEATURE_AVX2
#define CPU_FEATURE_SSE

#ifdef CPU_FEATURE_AVX2
constexpr auto SIMDWidth = 256;
#else
#ifdef CPU_FEATURE_AVX
constexpr auto SIMDWidth = 256;
#endif
#endif