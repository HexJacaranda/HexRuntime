#pragma once
#include "..\..\RuntimeAlias.h"
#include "Platform.h"

#define REG static constexpr Int8
#define MSK static constexpr UInt64
#define BR(REGISTER) (1ull << REGISTER)

namespace RTP::Register::X86
{
	template<UInt32 Width>
	struct RegisterSet
	{

	};

	template<>
	struct RegisterSet<Platform::Bit32>
	{
		REG AX = 0x00;
		REG CX = 0x01;
		REG DX = 0x02;
		REG BX = 0x03;
		REG SP = 0x04;
		REG BP = 0x05;
		REG SI = 0x06;
		REG DI = 0x07;

		MSK CommonRegisterMask = BR(AX) | BR(CX) | BR(DX) | BR(BX);
	};

	template<>
	struct RegisterSet<Platform::Bit64>
	{
		REG R8 = 0x08;
		REG R9 = 0x09;
		REG R10 = 0x0A;
		REG R11 = 0x0B;
		REG R12 = 0x0C;
		REG R13 = 0x0D;
		REG R14 = 0x0E;
		REG R15 = 0x0F;

		MSK CommonRegisterMask = RegisterSet<Platform::Bit32>::CommonRegisterMask | 
			BR(R8) | BR(R9) | BR(R10) | BR(R11) | BR(R12) | BR(R13) | BR(R14) | BR(R15);
	};
}
#undef REG