#pragma once
#include "..\..\..\RuntimeAlias.h"
#include "HexJITContext.h"

namespace RTJ::Hex
{
	class IHexJITFlow
	{
	public:
		virtual BasicBlock* PassThrough() = 0;
	};
}