#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTC
{
	struct TypeRepresentation
	{
		UInt32 TypeReference;
		/// <summary>
		/// Currently we use CoreType to be the modifier, representation of
		/// primitive types or category(Struct) 
		/// </summary>
		UInt8 CoreType;
	};
}