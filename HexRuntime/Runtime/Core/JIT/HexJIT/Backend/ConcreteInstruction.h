#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\LinkList.h"
#include "..\..\..\Platform\PlatformSpecialization.h"

namespace RTJ::Hex
{
	struct OperandKind
	{
		ETY = UInt8;
		VAL Unused = 0x00;
		//Virtual phase
		VAL VirtualRegister = 0x01;
		VAL Local = 0x02;
		VAL Argument = 0x03;

		//Realization phase
		VAL Register = 0x04;		
		VAL Immediate = 0x05;
		VAL SIB = 0x06;
	};

	struct ScaleIndexBase
	{
		Int32 Displacement;
		UInt8 Scale;
		UInt8 Index;
		UInt8 Base;
	};

	struct InstructionOperand
	{
		UInt8 Kind;
		union
		{
			UInt8 Register;
			UInt64 Immediate64;
			UInt32 Immediate32;
			Int16 VariableIndex;
			/// <summary>
			/// SIB support
			/// </summary>
			ScaleIndexBase SIB;
		};
	};

	struct ConcreteInstruction
	{
		RTP::PlatformInstruction* Instruction;
		/// <summary>
		/// Requires 8-bytes alignment (SegmentHeap guarantees 8 bytes) and destination should be put at first
		/// </summary>
		InstructionOperand* Operands;
	public:
		ETY = Int;
		VAL FlagMask = 0x7;
		VAL ShouldNotEmit = 0x1;
		VAL Store = 0x2;
		VAL Load = 0x4;

		InstructionOperand* GetOperands()const {
			return (InstructionOperand*)((Int)Operands & ~FlagMask);
		}
		void SetFlag(Int flag) {
			Operands = (InstructionOperand*)((Int)Operands | 0x1);
		}
		bool ShouldEmit()const {
			return ((Int)Operands | ShouldNotEmit);
		}
		bool IsStore()const {
			return ((Int)Operands | Store);
		}
		bool IsLoad()const {
			return ((Int)Operands | Load);
		}
	};
}