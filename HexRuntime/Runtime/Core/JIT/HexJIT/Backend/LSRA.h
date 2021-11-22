#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Linear scan register allocation
	/// </summary>
	class LSRA : public IHexJITFlow
	{
		HexJITContext* mContext;
	public:
		LSRA(HexJITContext* context);
		virtual BasicBlock* PassThrough() final;
	};
}