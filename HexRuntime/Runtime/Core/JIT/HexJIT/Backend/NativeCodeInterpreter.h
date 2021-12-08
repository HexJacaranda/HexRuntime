#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Memory\SegmentHeap.h"
#include "..\IR.h"
#include "CodeSequence.h"


namespace RTJ::Hex
{
	template<UInt32 Architecture, UInt32 Width>
	class NativeCodeInterpreter
	{
	public:
		NativeCodeInterpreter(RTMM::SegmentHeap* heap) {

		}
		CodeSequence* Interpret(TreeNode* node) {
			return nullptr;
		}
	};
}