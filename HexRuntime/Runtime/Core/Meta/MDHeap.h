#pragma once
#include "..\..\RuntimeAlias.h"
#include "MDRecords.h"

namespace RTM
{
	class MDPrivateHeap
	{
	public:
		UInt8* Allocate(Int32 count);
		UInt8* AllocateForCode(Int32 count);
		void Unload();
	};

	/// <summary>
	/// Metadata heap for store meta data
	/// </summary>
	class MDHeap
	{
	public:
		static MDPrivateHeap* GetPrivateHeap(MDToken assemblyToken);
	};
}