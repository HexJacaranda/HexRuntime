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
		static constexpr UInt16 Memory = 0x0001;
		static constexpr UInt16 Register = 0x0002;
		static constexpr UInt16 Immediate = 0x0004;

		static constexpr UInt16 Width32 = 0x0010;
		static constexpr UInt16 Width64 = 0x0020;

		/// <summary>
		/// For SIMD
		/// </summary>
		static constexpr UInt16 Width128 = 0x0040;
		static constexpr UInt16 Width256 = 0x0080;
		static constexpr UInt16 Width512 = 0x0100;

		static constexpr Int32 UnboundRegister = std::numeric_limits<Int32>::max();

		UInt16 Flags;
		Int32 Value;
	};

#define REG AddressConstraint::Register
#define MEM AddressConstraint::Memory
#define IMM AddressConstraint::Immediate

#define UNB AddressConstraint::UnboundRegister

#define ADR(RM, WIDTH, REGISTER) AddressConstraint { RM | AddressConstraint::Width##WIDTH, REGISTER  }
#define ADR_UN(RM, WIDTH) ADR(RM, WIDTH, UNB)

	/// <summary>
	/// Now we can only support opcode of addresses up to 4.
	/// </summary>
	struct PlatformInstructionConstraint
	{
		Int8 AddressCount;
		std::array<AddressConstraint, 4> AddressConstraints;
	};

#define INS_CNS(COUNT ,...) PlatformInstructionConstraint { COUNT , { __VA_ARGS__ } }

	struct PlatformInstruction
	{
		Int8 Length;
		PlatformInstructionConstraint* Constraint;
		UInt8* GetOpCodeSequence()const {
			return (UInt8*)(this + 1);
		}
	};

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
