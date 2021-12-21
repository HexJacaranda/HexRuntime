#include "Bit.h"
#include <mimalloc.h>
#include <intrin.h>

RT::UInt8 RT::Bit::LeftMostSetBit(UInt64 value)
{
	unsigned long ret = 0;
	_BitScanForward64(&ret, value);
	return ret;
}

void RT::Bit::SetZero(UInt64& value, UInt8 bitIndex)
{
	_bittestandreset64((long long*)&value, bitIndex);
}

void RT::Bit::SetOne(UInt64& value, UInt8 bitIndex)
{
	_bittestandset64((long long*)&value, bitIndex);
}

bool RT::Bit::TestAllZero(UInt64* value, Int32 count)
{
#ifdef SIMD256
	__m256i zeros = _mm256_setzero_si256();
	for (Int32 i = 0; i < count; i += 4)
	{
		__m256i origin = _mm256_loadu_si256((__m256i*)(value + i));
		auto mask = _mm256_cmpneq_epu64_mask(origin, zeros);
		if (mask > 0)
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
#ifdef SIMD256
	__m256i zeros = _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFll);
	for (Int32 i = 0; i < count; i += 4)
	{
		__m256i origin = _mm256_loadu_si256((__m256i*)(value + i));
		auto mask = _mm256_cmpneq_epu64_mask(origin, zeros);
		if (mask > 0)
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

void RT::BitSet::SetOne(Int32 index)
{
	Int32 intIndex = index / (8 * sizeof(UInt64));
	Int32 bitIndex = index % (8 * sizeof(UInt64));
	Bit::SetOne(mBits[intIndex], bitIndex);
}

void RT::BitSet::SetZero(Int32 index)
{
	Int32 intIndex = index / (8 * sizeof(UInt64));
	Int32 bitIndex = index % (8 * sizeof(UInt64));
	Bit::SetZero(mBits[intIndex], bitIndex);
}

bool RT::BitSet::Test(Int32 index)
{
	return false;
}

bool RT::BitSet::IsZero() const
{
	return Bit::TestAllOne(mBits, mIntCount);
}

bool RT::BitSet::IsOne() const
{
	return Bit::TestAllZero(mBits, mIntCount);
}

RT::Int32 RT::BitSet::Count() const
{
	return mCount;
}

RT::BitSet::BitSet(Int32 count)
{
	constexpr Int32 base = 8 * sizeof(UInt64);
	Int32 intCount = count / base;
	if (count % base != 0)
		intCount++;
	Int32 finalCount = intCount * base;

	mBits = (UInt64*)mi_malloc_aligned(intCount * sizeof(UInt64), sizeof(UInt64));
	mCount = count;
	mIntCount = intCount;
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
