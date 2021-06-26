#include "Interlocked.h"
#include <Windows.h>

bool RTC::Interlocked::DWCAS(InteriorPointer* address, InteriorPointer value, InteriorPointer* compareValue)
{
	
#ifdef X64
	Int64* values = (Int64*)&value;
	return InterlockedCompareExchange128((Int64*)address, values[1], values[0], (Int64*)compareValue);
#else
	return CAS((Int64*)address, value.Value, compareValue.Value) == compareValue;
#endif
}

RT::Int64 RTC::Interlocked::CAS(Int64* address, Int64 value, Int64 compareValue)
{
	return InterlockedCompareExchange64((volatile LONG64*)address, value, compareValue);
}

RT::Int32 RTC::Interlocked::CAS(RT::Int32* address, RT::Int32 value, RT::Int32 compareValue)
{
	return InterlockedCompareExchange((volatile LONG*)address, value, compareValue);
}

void* RTC::Interlocked::CAS(void** address, void* value, void* compareValue)
{
	return InterlockedCompareExchangePointer(address, value, compareValue);
}

RT::Int32 RTC::Interlocked::Increment(Int32* value)
{
	return InterlockedIncrement((volatile LONG*)value);
}

RT::Int64 RTC::Interlocked::Increment(Int64* value)
{
	return InterlockedIncrement64((volatile LONG64*)value);
}

RT::Int32 RTC::Interlocked::Decrement(Int32* value)
{
	return InterlockedDecrement((volatile LONG*)value);
}

RT::Int64 RTC::Interlocked::Decrement(Int64* value)
{
	return InterlockedDecrement64((volatile LONG64*)value);
}

RT::Int32 RTC::Interlocked::Add(Int32* value, Int32 toAdd)
{
	return InterlockedAdd((volatile LONG*)value, toAdd);
}

inline RT::Int64 RTC::Interlocked::Add(Int64* value, Int64 toAdd)
{
	return InterlockedAdd64((volatile LONG64*)value, toAdd);
}

RT::Int32 RTC::Interlocked::Or(Int32* value, Int32 toOr)
{
	return InterlockedOr((volatile LONG*)value, toOr);
}

RT::Int64 RTC::Interlocked::Or(Int64* value, Int64 toOr)
{
	return InterlockedOr64((volatile LONG64*)value, toOr);
}

RT::Int32 RTC::Interlocked::And(Int32* value, Int32 toAnd)
{
	return InterlockedAnd((volatile LONG*)value, toAnd);
}

RT::Int64 RTC::Interlocked::And(Int64* value, Int64 toAnd)
{
	return InterlockedAnd64((volatile LONG64*)value, toAnd);
}

RT::Int32 RTC::Interlocked::Xor(Int32* value, Int32 toXor)
{
	return InterlockedXor((volatile LONG*)value, toXor);
}

RT::Int64 RTC::Interlocked::Xor(Int64* value, Int64 toXor)
{
	return InterlockedXor64((volatile LONG64*)value, toXor);
}
