#pragma once
#include "..\..\..\RuntimeAlias.h"

namespace RTJ
{
	class EmitPageProvider
	{
	public:
		static UInt8* Allocate(Int32 size);
		static UInt8* Free(UInt8* origin);
		static UInt8* SetExecutable(UInt8* origin);
	};
}