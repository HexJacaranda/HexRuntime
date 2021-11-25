#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include <array>

namespace RTJ::Hex
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

		static constexpr Int16 UnboundRegister = -1;

		UInt16 Flags;
		Int16 Register;
	};

#define REG AddressConstraint::Register
#define MEM AddressConstraint::Memory
#define IMM AddressConstraint::Immediate

#define UNB AddressConstraint::UnboundRegister

#define ADR(RM, WIDTH, REGISTER) AddressConstraint { RM | AddressConstraint::Width##WIDTH, REGISTER  }
#define ADR(RM, WIDTH) ADR(RM, WIDTH, UNB)

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
}
