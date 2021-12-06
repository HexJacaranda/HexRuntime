#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"
#include "Platform.h"
#include <array>
#include <ranges>

namespace RTP
{
	struct AddressConstraint
	{
		UNDERLYING_TYPE(UInt16);

		VALUE(Memory) = 0x0001;
		VALUE(Register) = 0x0002;
		VALUE(Immediate) = 0x0004;

		VALUE(Width32) = 0x0010;
		VALUE(Width64) = 0x0020;

		/// <summary>
		/// For SIMD
		/// </summary>		
		VALUE(Width128) = 0x0040;
		VALUE(Width256) = 0x0080;
		VALUE(Width512) = 0x0100;

		UInt16 Flags;
		UInt64 RegisterAvaliableMask;
	};

#define REG AddressConstraint::Register
#define MEM AddressConstraint::Memory
#define IMM AddressConstraint::Immediate

#define ADR(RM, WIDTH, REGISTER_MASK) AddressConstraint { RM | AddressConstraint::Width##WIDTH, REGISTER_MASK  }

	/// <summary>
	/// Now we can only support opcode of addresses up to 3.
	/// The flag which is zero will be the end if there're less than 3 oprands
	/// </summary>
	struct PlatformInstructionConstraint
	{
		std::array<AddressConstraint, 3> AddressConstraints;
	};

#define INS_CNS(COUNT ,...) PlatformInstructionConstraint { COUNT , { __VA_ARGS__ } }

	struct PlatformInstruction
	{
		PlatformInstructionConstraint Constraint;
		/// <summary>
		/// Currently up to 4 bytes opcode
		/// </summary>
		std::array<UInt8, 4> Opcodes;
		UInt8 Length;
	};

	int x = sizeof(PlatformInstruction);

	struct PlatformCallingConvention
	{ 
		Int32 ArgumentCount;
		/* For address constraint here, theoretically only REG and MEM can be supported by common platforms. 
		* Specially, REG | MEM indicates that it's placed on stack and address passed in register (See some 
		* calling convetion in X86-X64)
		*/
		AddressConstraint* ArgumentPassway;
		AddressConstraint ReturnPassway;
	};
	
	class CallingArgumentType
	{
	public:
		UNDERLYING_TYPE(UInt32);

		VALUE(Integer) = 0x00000000;
		VALUE(Float) = 0x00000001;
		VALUE(SIMD) = 0x00000002;
	};

	struct PlatformCallingArgument
	{
		Int32 LayoutSize;
		UInt32 Type = CallingArgumentType::Integer;
	};

	template<CallingConventions convention, UInt32 platform>
	class PlatformCallingConventionProvider
	{
	public:
		template<class RangeT>
		static PlatformCallingConvention* GetConvention(RangeT&&)
		{
			return nullptr;
		}
	};
}
