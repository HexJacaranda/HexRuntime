#pragma once
#include "..\..\RuntimeAlias.h"
#include "MDRecords.h"

namespace RTME
{
	class MDPrivateHeap
	{
	public:
		UInt8* Allocate(Int32 count);
		UInt8* AllocateForCode(Int32 count);
		void Unload();
	};
}

void* operator new(size_t size, RTME::MDPrivateHeap* heap);