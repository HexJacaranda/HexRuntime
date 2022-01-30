#pragma once
#include "..\..\..\RuntimeAlias.h"

namespace RTJ
{
	class EmitPageProvider
	{
	public:
		static UInt8* Allocate(Int32 size);
		static void Free(UInt8* origin);
		static UInt8* SetExecutable(UInt8* origin, Int32 executableLength);
		static UInt8* SetReadOnly(UInt8* origin, Int32 readOnlyLength);
	};
}