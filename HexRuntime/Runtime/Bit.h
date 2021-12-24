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
		static bool TestAllZero(UInt64* value, Int32 count);
		static bool TestAllOne(UInt64* value, Int32 count);
		static bool TestAt(UInt64 value, UInt8 bitIndex);
	};

	class BitSet
	{
		UInt64* mBits;
		Int32 mCount;
		Int32 mIntCount;
	public:
		BitSet(Int32 count);
		void SetOne(Int32 index);
		void SetZero(Int32 index);
		bool Test(Int32 index);
		bool IsZero()const;
		bool IsOne()const;
		Int32 Count()const;
		~BitSet();
	};
}