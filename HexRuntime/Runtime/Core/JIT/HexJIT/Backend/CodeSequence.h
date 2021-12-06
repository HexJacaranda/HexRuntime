#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\LinkList.h"
#include "..\..\..\Platform\PlatformSpecialization.h"

namespace RTJ::Hex
{
	struct CodeSequence
	{
		Int32 LineCount;
		RTP::PlatformInstruction** Instructions;
		
	};
}