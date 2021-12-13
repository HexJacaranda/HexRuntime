#pragma once
#include "..\..\RuntimeAlias.h"
#include "Platform.h"

#define REGV static constexpr Int8
#define MSK static constexpr UInt64
#define BR(REGVISTER) (1ull << REGVISTER)

namespace RTP::Register::X86
{
	struct RegisterSetBase
	{
		REGV AX = 0x00;
		REGV CX = 0x01;
		REGV DX = 0x02;
		REGV BX = 0x03;
		REGV SP = 0x04;
		REGV BP = 0x05;
		REGV SI = 0x06;
		REGV DI = 0x07;

		REGV R8 = 0x08;
		REGV R9 = 0x09;
		REGV R10 = 0x0A;
		REGV R11 = 0x0B;
		REGV R12 = 0x0C;
		REGV R13 = 0x0D;
		REGV R14 = 0x0E;
		REGV R15 = 0x0F;

		REGV XMMBase = 0x08;
		REGV XMM0 = 0x08;
		REGV XMM1 = 0x09;
		REGV XMM2 = 0x0A;
		REGV XMM3 = 0x0B;
		REGV XMM4 = 0x0C;
		REGV XMM5 = 0x0D;
		REGV XMM6 = 0x0E;
		REGV XMM7 = 0x0F;

		REGV XMM8 = 0x10;
		REGV XMM9 = 0x11;
		REGV XMM10 = 0x12;
		REGV XMM11 = 0x13;
		REGV XMM12 = 0x14;
		REGV XMM13 = 0x15;
		REGV XMM14 = 0x16;
		REGV XMM15 = 0x17;
	};

	template<UInt32 Width>
	struct RegisterSet
	{

	};

	template<>
	struct RegisterSet<Platform::Bit32> : public RegisterSetBase
	{
		MSK CommonRegisterMask = BR(AX) | BR(CX) | BR(DX) | BR(BX) |
			BR(XMM0) | BR(XMM1) | BR(XMM2) | BR(XMM3) | BR(XMM4) | BR(XMM5) | BR(XMM6) | BR(XMM7);
	};

	template<>
	struct RegisterSet<Platform::Bit64> : public RegisterSetBase
	{
		MSK CommonRegisterMask = RegisterSet<Platform::Bit32>::CommonRegisterMask | 
			BR(R8) | BR(R9) | BR(R10) | BR(R11) | BR(R12) | BR(R13) | BR(R14) | BR(R15) |
			BR(XMM8) | BR(XMM9) | BR(XMM10) | BR(XMM11) | BR(XMM12) | BR(XMM13) | BR(XMM14) | BR(XMM15);
	};
}
#undef REGV