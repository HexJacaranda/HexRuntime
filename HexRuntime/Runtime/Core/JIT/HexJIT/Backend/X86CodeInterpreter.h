#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "NativeCodeInterpreter.h"
#include "..\..\..\Platform\Platform.h"

namespace RTJ::Hex
{
	template<UInt32 Width>
	class NativeCodeInterpreter<Platform::Platform::x86, Width>
	{
		RTMM::SegmentHeap* mHeap;
	public:
		NativeCodeInterpreter(RTMM::SegmentHeap* heap) : mHeap(heap)
		{

		}

		template<InstructionGenerationEventT Fn>
		void Interpret(TreeNode* node, Fn&& instructionGenerationEvent)
		{

		}

		template<InstructionGenerationEventT Fn>
		void InterpretBranch(TreeNode* branchValue, Fn&& instructionGenerationEvent)
		{

		}

		ConcreteInstruction ProvideStore(UInt16 variableIndex, UInt8 physicalRegister)
		{
			return {};
		}

		ConcreteInstruction ProvideLoad(UInt16 variableIndex, UInt8 physicalRegister)
		{
			return {};
		}

		bool PurposeMemoryOperation(ConcreteInstruction& toModify, Int32 index, UInt16 variable)
		{
			return {};
		}
	};
}