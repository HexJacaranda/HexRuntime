#pragma once
#include "..\RuntimeAlias.h"
#include "InteriorPointer.h"

namespace RTC
{
	namespace Link
	{
		//Double word compare and swap
		extern "C" bool __stdcall DWCAS(InteriorPointer * address, InteriorPointer value, InteriorPointer compareValue);
	}

	class Interlocked
	{
	public:
		ForcedInline static bool __stdcall DWCAS(InteriorPointer* address, InteriorPointer value, InteriorPointer compareValue);
	};
}