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
		//Used for calling convention only
		VAL SpecificRegister = 0x0008;

		/// <summary>
		/// Width Bit
		/// </summary>
		VAL Width8 = 0x0010;
		VAL Width16 = 0x0011;
		VAL Width32 = 0x0012;
		VAL Width64 = 0x0013;

		/// <summary>
		/// For SIMD
		/// </summary>		
		VAL Width128 = 0x0014;
		VAL Width256 = 0x0015;
		VAL Width512 = 0x0016;

		VAL Compound = 0x7000;
	};

	struct AddressConstraint
	{
		AddressConstraint(UInt64 mask, UInt16 flags) :
			RegisterAvaliableMask(mask),
			Flags(flags) {}
		AddressConstraint(UInt8 single, UInt16 flags) :
			SingleRegister(single),
			Flags(flags) {}
		AddressConstraint(UInt16 flags) :
			Flags(flags),
			RegisterAvaliableMask(0) {}
		AddressConstraint() {}

		union {
			UInt64 RegisterAvaliableMask;
			UInt8 SingleRegister;
			AddressConstraint* SubConstraints = nullptr;
		};		
		/// <summary>
		/// Reserved for register mask
		/// </summary>
		PADDING_EXT(5);
		UInt8 CompundLength = 0;
		UInt16 Flags = 0;
	};

#define REG_C RTP::AddressConstraintFlags::Register
#define SREG_C RTP::AddressConstraintFlags::SpecificRegister
#define MEM_C RTP::AddressConstraintFlags::Memory
#define IMM_C RTP::AddressConstraintFlags::Immediate

#define W8 RTP::AddressConstraintFlags::Width8
#define W16 RTP::AddressConstraintFlags::Width16
#define W32 RTP::AddressConstraintFlags::Width32
#define W64 RTP::AddressConstraintFlags::Width64

#define ADR_MEM(WIDTH) RTP::AddressConstraint { WIDTH | MEM_C }
#define ADR_REG(MSK, WIDTH) RTP::AddressConstraint { MSK, WIDTH | REG_C }
#define ADR_REG_MEM(MSK, WIDTH) RTP::AddressConstraint { MSK, WIDTH | REG_C | MEM_C }

#define OPCD(OP_VAL) {OP_VAL}, 1
#define OPCD_2(OP_VAL1, OP_VAL2) { OP_VAL1, OP_VAL2 }, 2
#define OPCD_3(OP_VAL1, OP_VAL2, OP_VAL3) { OP_VAL1, OP_VAL2, OP_VAL3 }, 3
#define OPCD_4(OP_VAL1, OP_VAL2, OP_VAL3, OP_VAL4) { OP_VAL1, OP_VAL2, OP_VAL3, OP_VAL4 }, 4

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

#define PINST RTP::PlatformInstruction

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
