#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Reducing IR from SSA to normal form
	/// </summary>
	class SSAReducer : public IHexJITFlow
	{
	public:
		virtual BasicBlock* PassThrough() final;
	};
}