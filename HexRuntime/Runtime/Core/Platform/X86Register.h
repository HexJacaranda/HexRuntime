#pragma once
#include "..\..\RuntimeAlias.h"
#include "Platform.h"

#define REG static constexpr Int8
#define MSK static constexpr UInt64
#define BR(REGISTER) (1ull << REGISTER)

namespace RTP::Register::X86
{
	template<UInt32 Width>
	struct RegisterSetBase {};

	template<>
	struct RegisterSetBase<Platform::Bit32>
	{
		REG AX = 0x00;
		REG CX = 0x01;
		REG DX = 0x02;
		REG BX = 0x03;
		REG SP = 0x04;
		REG BP = 0x05;
		REG SI = 0x06;
		REG DI = 0x07;

		REG XMM0 = 0X10;
		REG XMM1 = 0X11;
		REG XMM2 = 0X12;
		REG XMM3 = 0X13;
		REG XMM4 = 0X14;
		REG XMM5 = 0X15;
		REG XMM6 = 0X16;
		REG XMM7 = 0X17;

		static constexpr UInt8 RegisterCount = XMM7 + 1;
	};

	template<>
	struct RegisterSetBase<Platform::Bit64> : public RegisterSetBase<Platform::Bit32>
	{
		REG R8 = 0x08;
		REG R9 = 0x09;
		REG R10 = 0x0A;
		REG R11 = 0x0B;
		REG R12 = 0x0C;
		REG R13 = 0x0D;
		REG R14 = 0x0E;
		REG R15 = 0x0F;

		REG XMM8 = 0X18;
		REG XMM9 = 0X19;
		REG XMM10 = 0X1A;
		REG XMM11 = 0X1B;
		REG XMM12 = 0X1C;
		REG XMM13 = 0X1D;
		REG XMM14 = 0X1E;
		REG XMM15 = 0X1F;

		static constexpr UInt8 RegisterCount = XMM15 + 1;
	};

	template<UInt32 Width>
	struct RegisterSet {};

	template<>
	struct RegisterSet<Platform::Bit32> : public RegisterSetBase<Platform::Bit32>
	{
		MSK Common = BR(AX) | BR(CX) | BR(DX) | BR(BX) | BR(SI) | BR(DI);
		MSK InsideOpcode = Common;
		MSK CommonXMM = BR(XMM0) | BR(XMM1) | BR(XMM2) | BR(XMM3) | BR(XMM4) | BR(XMM5) | BR(XMM6) | BR(XMM7);
		MSK InsideOpcodeXMM = CommonXMM;
	};

	template<>
	struct RegisterSet<Platform::Bit64> : public RegisterSetBase<Platform::Bit64>
	{
		MSK Common = RegisterSet<Platform::Bit32>::Common | 
			BR(R8) | BR(R9) | BR(R10) | BR(R11) | BR(R12) | BR(R13) | BR(R14) | BR(R15);
		MSK InsideOpcode = RegisterSet<Platform::Bit32>::Common;

		MSK CommonXMM = RegisterSet<Platform::Bit32>::CommonXMM |
			BR(XMM8) | BR(XMM9) | BR(XMM10) | BR(XMM11) | BR(XMM12) | BR(XMM13) | BR(XMM14) | BR(XMM15);
		MSK InsideOpcodeXMM = RegisterSet<Platform::Bit32>::InsideOpcodeXMM;
	};
}

#undef REG
#undef MSK