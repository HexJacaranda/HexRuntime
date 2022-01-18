#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"
#include "..\Memory\PrivateHeap.h"
#include "Platform.h"
#include <array>
#include <ranges>

namespace RTP
{
	struct AddressConstraintFlags
	{
	public:
		ETY = UInt16;

		VAL Memory = 0x0001;
		VAL Register = 0x0002;
		VAL Immediate = 0x0004;
		//Used for calling convention marking
		VAL SpecificRegister = 0x0008;

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
		union {
			UInt64 RegisterAvaliableMask;
			UInt8 SingleRegister;
		};		
		/// <summary>
		/// Reserved for register mask
		/// </summary>
		PADDING_EXT(6);
		UInt16 Flags;	
	};

#define REG AddressConstraintFlags::Register
#define SREG AddressConstraintFlags::SpecificRegister
#define MEM AddressConstraintFlags::Memory
#define IMM AddressConstraintFlags::Immediate

#define ADR(RM, WIDTH, REGISTER_MASK) AddressConstraint { RM | AddressConstraint::Width##WIDTH, REGISTER_MASK  }

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
		UInt8 OpcodeLength;
		UInt8 ConstraintLength;
	};

	struct PlatformCallingConvention
	{ 
		Int32 ArgumentCount;
		/* For address constraint here, theoretically only SREG and MEM can be supported by common platforms. 
		* Specially, SREG | MEM indicates that it's placed on stack and address passed in register (See some 
		* calling convetions in X86-64)
		*/
		AddressConstraint* ArgumentPassway;
		AddressConstraint ReturnPassway;
	};
	
	class CallingArgumentType
	{
	public: 
		ETY = UInt32;

		VAL Integer = 0x00000000;
		VAL Float = 0x00000001;
		VAL Struct = 0x00000002;
	};

	struct PlatformCallingArgument
	{
		Int32 LayoutSize = 0;
		UInt32 Type = CallingArgumentType::Integer;
	};

	template<CallingConventions convention, PLATFORM_TEMPLATE_ARGS>
	class PlatformCallingConventionProvider
	{
	public:
		PlatformCallingConventionProvider(RTMM::PrivateHeap* heap) {

		}
		template<class RangeT>
		PlatformCallingConvention* GetConvention(RangeT&&)
		{
			return nullptr;
		}
	};
}
