#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Memory\SegmentHeap.h"
#include "..\IR.h"
#include "ConcreteInstruction.h"
#include <vector>

namespace RTJ::Hex
{
	template<UInt32 Architecture, UInt32 Width>
	class NativeCodeInterpreter
	{
	public:
		NativeCodeInterpreter(RTMM::SegmentHeap* heap) {

		}
		void Interpret(std::vector<ConcreteInstruction>& out, TreeNode* node) {
		}
	};
}