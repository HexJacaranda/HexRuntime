#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Linearizer the materialized IR to sequential order of trees that have
	/// at most depth of two
	/// </summary>
	class Linearizer
	{
		HexJITContext* mJITContext;
	public:

	};
}