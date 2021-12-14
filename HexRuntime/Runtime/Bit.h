#pragma once
#include "RuntimeAlias.h"

namespace RT
{
	class Bit
	{
	public:
		static UInt8 LeftMostSetBit(UInt64 value);
		static void SetZero(UInt64& value, UInt8 bitIndex);
		static void SetOne(UInt64& value, UInt8 bitIndex);
	};
}