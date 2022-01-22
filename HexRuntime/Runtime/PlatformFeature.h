#pragma once
#include "RuntimeAlias.h"

#define AVX
#define AVX2
#define SSE

#ifdef AVX2
constexpr auto SIMDWidth = 256;
#else
#ifdef AVX
constexpr auto SIMDWidth = 256;
#endif
#endif