#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"
#include "Platform.h"
#include <array>
#include <ranges>

namespace RTP
{
	struct AddressConstraintFlags
	{
		ETY = UInt16;

		VAL Unused = 0x0000;
		VAL Memory = 0x0001;
		VAL Register = 0x0002;
		VAL Immediate = 0x0004;

		VAL Width32 = 0x0010;
		VAL Width64 = 0x0020;

		/// <summary>
		/// For SIMD
		/// </summary>		
		VAL Width128 = 0x0040;
		VAL Width256 = 0x0080;
		VAL Width512 = 0x0100;
	};

	struct AddressConstraint
	{
		UInt16 Flags;
		UInt64 RegisterAvaliableMask;
	};

#define NON AddressConstraintFlags::Unused
#define REG AddressConstraintFlags::Register
#define MEM AddressConstraintFlags::Memory
#define IMM AddressConstraintFlags::Immediate

#define ADR(RM, WIDTH, REGISTER_MASK) AddressConstraint { RM | WIDTH, REGISTER_MASK  }
#define NO_RET AddressConstraint { NON, 0 }

#define INS_CNS(COUNT ,...) PlatformInstructionConstraint { COUNT , { __VA_ARGS__ } }

	struct PlatformInstruction
	{
		/// <summary>
		/// Now we can only support opcode of addresses up to 3.
		/// The flag which is zero will be the end if there're less than 3 operands
		/// </summary>
		std::array<AddressConstraint, 3> AddressConstraints;
		/// <summary>
		/// Currently up to 4 bytes opcode
		/// </summary>
		std::array<UInt8, 4> Opcodes;
		UInt8 Length;
	};

	struct PlatformCallingConvention
	{ 
		Int32 ArgumentCount;
		/* For address constraint here, theoretically only REG and MEM can be supported by common platforms. 
		* Specially, REG | MEM indicates that it's placed on stack and address passed in register (See some 
		* calling convetions in X86-X64)
		*/
		AddressConstraint* ArgumentPassway;
		AddressConstraint ReturnPassway;
	public:

	};
	
	class CallingArgumentType
	{
	public:
		ETY = UInt16;
		VAL EmptyReturn = 0x0000;
		VAL Integer = 0x0001;
		VAL Float = 0x0002;
		VAL SIMD = 0x0003;
	};

	/// <summary>
	/// Any argument greater than 0x7FFF is set to 0x7FFF for it's useless in calling conv generation
	/// </summary>
	struct PlatformCallingArgument
	{
		ETY = Int16;
		VAL LimitSize = 0x7FFF;

		Int16 LayoutSize;
		UInt16 Type = CallingArgumentType::Integer;
	};

	template<CallingConventions convention, 
		UInt32 width,
		UInt32 os,
		UInt32 architecture>
	class PlatformCallingConventionProvider
	{
	public:
		template<class RangeT>
		static PlatformCallingConvention* GetConvention(PlatformCallingConvention* callingConv, RangeT&&)
		{
			return nullptr;
		}
	};

#define GET_CONV_ENTRACE(OUT_CALLING_CONV_NAME, IN_ARGUMENTS) \
			template<class RangeT> \
			static PlatformCallingConvention* GetConvention(PlatformCallingConvention* OUT_CALLING_CONV_NAME, RangeT&& IN_ARGUMENTS)
}
