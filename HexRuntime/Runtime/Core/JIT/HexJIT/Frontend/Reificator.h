#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Responsible for replacing high-level nodes with primitive low-level nodes
	/// </summary>
	class Reificator
	{
		HexJITContext* mJITContext;
	public:
		Reificator(HexJITContext* context);
	};
}