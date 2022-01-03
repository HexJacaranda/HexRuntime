#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\Memory\SegmentHeap.h"
#include "..\IR.h"
#include "ConcreteInstruction.h"

namespace RTJ::Hex
{
	template<class Fn>
	concept InstructionGenerationEventT = requires (ConcreteInstruction ins, Fn && action)
	{
		std::forward<Fn>(action)(ins);
	};

	/// <summary>
	/// For exposed instruction flag, see comments in ConcreteInstruction 
	/// </summary>
	template<UInt32 Architecture, UInt32 Width>
	class NativeCodeInterpreter
	{
	public:
		NativeCodeInterpreter(RTMM::SegmentHeap* heap) 
		{

		}

		/// <summary>
		/// Interpret normal tree
		/// </summary>
		/// <param name="node"></param>
		/// <param name="instructionGenerationEvent"></param>
		template<InstructionGenerationEventT Fn>
		void Interpret(TreeNode* node, Fn&& instructionGenerationEvent)
		{

		}

		/// <summary>
		/// Interpret (un)conditional jump
		/// </summary>
		/// <param name="branchValue">when this is null, it indicates an unconditional branch</param>
		/// <param name="instructionGenerationEvent"></param>
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

		bool PurposeMemoryOperation(ConcreteInstruction const& origin, ConcreteInstruction& out)
		{
			return {};
		}
	};

#define ASM(EXPR) std::forward<Fn>(instructionGenerationEvent)(EXPR)
#define ASM_LD(EXPR) ASM((EXPR).SetFlag(ConcreteInstruction::LocalLoad))
#define ASM_ST(EXPR) ASM((EXPR).SetFlag(ConcreteInstruction::LocalStore))
}