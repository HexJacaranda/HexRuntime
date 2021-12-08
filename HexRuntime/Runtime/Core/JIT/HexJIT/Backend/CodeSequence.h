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
		VAL Register = 0x01;
		VAL VirtualRegister = 0x02;
		VAL Immediate = 0x03;
		VAL SIB = 0x04;
	};

	struct InstructionOperand
	{
		UInt8 Kind;
		union
		{
			UInt8 Register;
			UInt64 Immediate64;
			UInt32 Immediate32;
			UInt64 SIB;
		};
	};

	struct SequenceLine
	{
		RTP::PlatformInstruction* Instruction;
		/// <summary>
		/// Requires at least 4-bytes alignment (SegmentHeap guarantees 8 bytes) and destination should be put at first
		/// </summary>
		InstructionOperand* Operands;
	public:
		static constexpr Int FlagMask = 0x3;
		static constexpr Int ShouldNotEmit = 0x1;
		static constexpr Int StorePoint = 0x2;
		InstructionOperand* GetOperands()const {
			return (InstructionOperand*)((Int)Operands & ~FlagMask);
		}
		void SetFlag(Int flag) {
			Operands = (InstructionOperand*)((Int)Operands | 0x1);
		}
		bool ShouldEmit()const {
			return ((Int)Operands | ShouldNotEmit);
		}
		bool IsStorePoint()const {
			return ((Int)Operands | StorePoint);
		}
	};

	struct CodeSequence
	{
		Int32 LineCount;
		SequenceLine* Instructions;	
	};
}