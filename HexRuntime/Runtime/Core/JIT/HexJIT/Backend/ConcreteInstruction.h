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
		VAL Immediate32 = 0x05;
		VAL Immediate64 = 0x06;
	};

	struct InstructionOperandFlags
	{
		ETY = UInt8;
		VAL Modify = 0x01;
	};

	struct InstructionOperand
	{
		UInt8 Kind;
		UInt8 Flags;
		//These kinds of values are directly supported by our model
		union
		{		
			UInt8 Register;
			UInt8 VirtualRegister;
			UInt16 VariableIndex;
			UInt64 Immediate64;
			UInt32 Immediate32;
			//Support for extension format
			void* Extension;
		};
	public:
		bool IsModifyingRegister()const {
			return Flags & InstructionOperandFlags::Modify;
		}
	};

	/// <summary>
	///	Concrete instruction consists of memory restriction exposed platform instruction and
	/// the concrete operand(s), although they may be actually virtual.
	/// Special flags:
	/// <para> 
	/// 1. LocalStore: When your instruction store a register to a local variable (arugments), set this flag.
	/// And make sure operands is [Local Variable, Reg]
	/// </para>
	/// <para> 
	/// 2. LocalLoad: When your instruction load a local variable (arugments) to a register, set this flag. 
	/// And make sure operands is [Reg, Local Variable] 
	/// </para>
	/// <para> 
	/// And these two flags help LSRA to do more optimization and mark ShoudNotEmit to reduce unnecessary 
	/// memory access from template code.
	/// </para>
	/// </summary>
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
		VAL LocalStore = 0x2;
		VAL LocalLoad = 0x4;

		InstructionOperand* GetOperands()const {
			return (InstructionOperand*)((Int)Operands & ~FlagMask);
		}
		ConcreteInstruction* SetFlag(Int flag) {
			Operands = (InstructionOperand*)((Int)Operands | flag);
			return this;
		}
		Int GetFlag()const {
			return (Int)Operands & FlagMask;
		}
		bool ShouldEmit()const {
			return ((Int)Operands | ShouldNotEmit);
		}
		bool IsLocalStore()const {
			return ((Int)Operands | LocalStore);
		}
		bool IsLocalLoad()const {
			return ((Int)Operands | LocalLoad);
		}
	};
}