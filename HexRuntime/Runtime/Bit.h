#pragma once
#include "RuntimeAlias.h"
#include "PlatformFeature.h"

namespace RT
{
	class Bit
	{
	public:
		static constexpr auto SIMDWidth = ::SIMDWidth;
		static constexpr UInt8 InvalidBit = std::numeric_limits<UInt8>::max();
		static UInt8 LeftMostSetBit(UInt64 value);
		static UInt8 RightMostSetBit(UInt64 value);
		static bool SetZero(UInt64& value, UInt8 bitIndex);
		static bool SetOne(UInt64& value, UInt8 bitIndex);
		static bool TestAllZero(UInt64* value, Int32 count);
		static bool TestAllOne(UInt64* value, Int32 count);
		static bool TestAt(UInt64 value, UInt8 bitIndex);
	};

	class BitSet
	{
		UInt64* mBits;
		Int32 mCount;
		Int32 mPackedIntCount;
		Int32 mIntCount = 0;
		Int32 mOneCount = 0;
	public:
		BitSet(Int32 count);
		void SetOne(Int32 index);
		void SetZero(Int32 index);
		Int32 ScanSmallestSetIndex();
		Int32 ScanBiggestSetIndex();
		Int32 ScanSmallestUnsetIndex();
		Int32 ScanBiggestUnsetIndex();
		bool Test(Int32 index);
		bool IsZero()const;
		bool IsOne()const;
		Int32 Count()const;
		~BitSet();
	};
}