#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\ExecutionEngine\StackCrawling.h"
#include "..\..\..\Memory\PrivateHeap.h"
#include "..\HexJITContext.h"

namespace RTJ::Hex
{
	class StackFrameGenerator
	{
		RTMM::PrivateHeap* mAssemblyHeap;
		HexJITContext* mContext;
		static constexpr Int32 Alignment = sizeof(UInt32);
	public:
		StackFrameGenerator(HexJITContext* context);
		RTEE::StackFrameInfo* Generate();
	};
}              