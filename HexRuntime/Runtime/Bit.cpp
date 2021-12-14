#include "Bit.h"
#include <intrin.h>

inline RT::UInt8 RT::Bit::LeftMostSetBit(UInt64 value)
{
	unsigned long ret = 0;
	_BitScanForward64(&ret, value);
	return ret;
}

inline void RT::Bit::SetZero(UInt64& value, UInt8 bitIndex)
{
	_bittestandreset64((long long*)&value, bitIndex);
}

inline void RT::Bit::SetOne(UInt64& value, UInt8 bitIndex)
{
	_bittestandset64((long long*)&value, bitIndex);
}
