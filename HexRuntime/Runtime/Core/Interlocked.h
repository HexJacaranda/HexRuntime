#pragma once
#include "..\RuntimeAlias.h"
#include "InteriorPointer.h"

namespace RTC
{
	class Interlocked
	{
	public:
		static bool DWCAS(RTC::InteriorPointer* address, RTC::InteriorPointer value, RTC::InteriorPointer* compareValue);
		static RT::Int64 CAS(RT::Int64* address, RT::Int64 value, RT::Int64 compareValue);
		static RT::Int32 CAS(RT::Int32* address, RT::Int32 value, RT::Int32 compareValue);
		static void* CAS(void** address, void* value, void* compareValue);
		static RT::Int32 Increment(RT::Int32* value);
		static RT::Int64 Increment(RT::Int64* value);
		static RT::Int32 Decrement(RT::Int32* value);
		static RT::Int64 Decrement(RT::Int64* value);
		static RT::Int32 Add(RT::Int32* value, RT::Int32 toAdd);
		static RT::Int64 Add(RT::Int64* value, RT::Int64 toAdd);
		static RT::Int32 Or(RT::Int32* value, RT::Int32 toOr);
		static RT::Int64 Or(RT::Int64* value, RT::Int64 toOr);
		static RT::Int32 And(RT::Int32* value, RT::Int32 toAnd);
		static RT::Int64 And(RT::Int64* value, RT::Int64 toAnd);
		static RT::Int32 Xor(RT::Int32* value, RT::Int32 toXor);
		static RT::Int64 Xor(RT::Int64* value, RT::Int64 toXor);
	};
}