#include "Bit.h"
#include <mimalloc.h>
#include <intrin.h>
#include "PlatformFeature.h"

RT::UInt8 RT::Bit::LeftMostSetBit(UInt64 value)
{
	unsigned long ret = 0;
	if (_BitScanForward64(&ret, value) == 0)
		return InvalidBit;
	return ret;
}

RT::UInt8 RT::Bit::RightMostSetBit(UInt64 value)
{
	unsigned long ret = 0;
	if (_BitScanReverse64(&ret, value) == 0)
		return InvalidBit;
	return ret;
}

bool RT::Bit::SetZero(UInt64& value, UInt8 bitIndex)
{
	return _bittestandreset64((long long*)&value, bitIndex);
}

bool RT::Bit::SetOne(UInt64& value, UInt8 bitIndex)
{
	return _bittestandset64((long long*)&value, bitIndex);
}

bool RT::Bit::TestAllZero(UInt64* value, Int32 count)
{
#ifdef AVX
	for (Int32 i = 0; i < count; i += 4)
	{
		__m256i origin = _mm256_loadu_si256((__m256i*)(value + i));
		if (!_mm256_testz_si256(origin, origin))
			return false;
	}
	return true;
#else
	for (Int32 i = 0; i < count; ++i)
	{
		if (value[i] != 0)
			return false;
	}
	return true;
#endif
}

bool RT::Bit::TestAllOne(UInt64* value, Int32 count)
{
#ifdef AVX
	__m256i allOne = _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFll);
	for (Int32 i = 0; i < count; i += 4)
	{
		__m256i origin = _mm256_loadu_si256((__m256i*)(value + i));
		if (!_mm256_testc_si256(origin, allOne))
			return false;
	}
	return true;
#else
	for (Int32 i = 0; i < count; ++i)
	{
		if (value[i] != 0xFFFFFFFFFFFFFFFFull)
			return false;
	}
	return true;
#endif
}

bool RT::Bit::TestAt(UInt64 value, UInt8 bitIndex)
{
	return _bittest64((long long*)&value, bitIndex);
}

void RT::BitSet::SetOne(Int32 index)
{
	Int32 intIndex = index / (8 * sizeof(UInt64));
	Int32 bitIndex = index % (8 * sizeof(UInt64));
	mOneCount += !Bit::SetOne(mBits[intIndex], bitIndex);
}

void RT::BitSet::SetZero(Int32 index)
{
	Int32 intIndex = index / (8 * sizeof(UInt64));
	Int32 bitIndex = index % (8 * sizeof(UInt64));
	mOneCount -= Bit::SetZero(mBits[intIndex], bitIndex);
}

RT::Int32 RT::BitSet::ScanSmallestSetIndex()
{
	for (Int32 i = 0; i < mIntCount; ++i)
		if (auto index = Bit::LeftMostSetBit(mBits[i]); index != Bit::InvalidBit)
			return i * sizeof(UInt64) * 8 + index;

	return -1;
}

RT::Int32 RT::BitSet::ScanBiggestSetIndex()
{
	for (Int32 i = mIntCount - 1; i >= 0; --i)
		if (auto index = Bit::RightMostSetBit(mBits[i]); index != Bit::InvalidBit)
			return i * sizeof(UInt64) * 8 + index;

	return -1;
}

RT::Int32 RT::BitSet::ScanSmallestUnsetIndex()
{
	constexpr auto UInt64Bits = 8 * sizeof(UInt64);
	Int32 completeInts = mCount / UInt64Bits;
	Int32 remainAccessibleBits = mCount % UInt64Bits;

	for (Int32 i = 0; i < completeInts; ++i)
		if (auto index = Bit::LeftMostSetBit(~mBits[i]); index != Bit::InvalidBit)
			return i * UInt64Bits + index;

	if (remainAccessibleBits > 0)
	{
		UInt64 mask = 0xFFFFFFFFFFFFFFFFull;
		mask <<= remainAccessibleBits;
		mask = ~mask;

		UInt64 toScan = (~mBits[completeInts]) & mask;
		if (auto index = Bit::LeftMostSetBit(toScan); index != Bit::InvalidBit)
			return completeInts * UInt64Bits + index;
	}

	return -1;
}

RT::Int32 RT::BitSet::ScanBiggestUnsetIndex()
{
	constexpr auto UInt64Bits = 8 * sizeof(UInt64);
	Int32 completeInts = mCount / UInt64Bits;
	Int32 remainAccessibleBits = mCount % UInt64Bits;

	if (remainAccessibleBits > 0)
	{
		UInt64 mask = 0xFFFFFFFFFFFFFFFFull;
		mask <<= remainAccessibleBits;
		mask = ~mask;

		if (auto index = Bit::RightMostSetBit((~mBits[completeInts]) & mask); index != Bit::InvalidBit)
			return completeInts * UInt64Bits + index;
	}

	for (Int32 i = completeInts - 1; i >= 0; --i)
		if (auto index = Bit::RightMostSetBit(~mBits[i]); index != Bit::InvalidBit)
			return i * UInt64Bits + index;

	return -1;
}

bool RT::BitSet::Test(Int32 index)
{
	Int32 intIndex = index / (8 * sizeof(UInt64));
	Int32 bitIndex = index % (8 * sizeof(UInt64));
	return Bit::TestAt(mBits[intIndex], bitIndex);
}

bool RT::BitSet::IsZero() const
{
	return Bit::TestAllZero(mBits, mPackedIntCount);
}

bool RT::BitSet::IsOne() const
{
	return mCount == mOneCount;
}

RT::Int32 RT::BitSet::Count() const
{
	return mCount;
}

RT::BitSet::BitSet(Int32 count)
{
	constexpr auto UInt64Bits = 8 * sizeof(UInt64);
	constexpr auto CountOfPackedUInt64 = Bit::SIMDWidth / UInt64Bits;
	constexpr auto Base = Bit::SIMDWidth;

	Int32 packUnit = count / Base;
	if (count % Base != 0)
		packUnit++;

	Int32 intCount = packUnit * CountOfPackedUInt64;

	mBits = (UInt64*)mi_malloc_aligned(intCount * sizeof(UInt64), sizeof(UInt64));
	std::memset(mBits, 0, intCount * sizeof(UInt64));
	mCount = count;
	mPackedIntCount = intCount;

	mIntCount = count / UInt64Bits;
	if (count % UInt64Bits != 0)
		mIntCount++;
}

RT::BitSet::~BitSet()
{
	if (mBits != nullptr)
	{
		mi_free_aligned(mBits, sizeof(UInt64));
		mBits = nullptr;
		mCount = 0;
	}
}
