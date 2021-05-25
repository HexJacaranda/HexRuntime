#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTI
{
	/// <summary>
	/// Provides PAL for memory
	/// </summary>
	class OSMemory
	{
	public:
		static void* Allocate(Int space, bool executable = false);
		static void* Commit(void* target, Int size);
		static void* Decommit(void* target, Int size);
	};
}