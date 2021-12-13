#pragma once
#include "..\..\RuntimeAlias.h"
#include "PlatformSpecialization.h"
#include "X86Register.h"

namespace RTP
{
	template<UInt32 OS> 
	class PlatformCallingConventionProvider<
		CallingConventions::JIT, 
		Platform::Bit32, 
		OS, 
		Platform::x86>
	{
	public:
		GET_CONV_ENTRACE(callingConv, range)
		{

		}
	};

	template<UInt32 OS>
	class PlatformCallingConventionProvider<
		CallingConventions::JIT, 
		Platform::Bit64, 
		OS,
		Platform::x86>
	{
		ETY = Int32;
		VAL IntegerSizeLimit = 8;
		VAL FloatSizeLimit = 16;

		using Registers = Register::X86::RegisterSet<Platform::Bit64>;
	public:
		GET_CONV_ENTRACE(callingConv, range)
		{
			//Firstly for return type
			PlatformCallingArgument retArg = (range | std::views::take(1)).front();
			if (retArg.Type == CallingArgumentType::EmptyReturn)
				callingConv->ReturnPassway = NO_RET;
			else
			{
				switch (retArg.Type)
				{
				case CallingArgumentType::Integer:
					if (retArg.LayoutSize <= IntegerSizeLimit)
						callingConv->ReturnPassway = ADR(REG, Platform::Bit64, BR(Registers::AX));
					else
						callingConv->ReturnPassway = ADR(REG | MEM, Platform::Bit64, BR(Registers::AX));
					break;
				case CallingArgumentType::Float:
					if (retArg.LayoutSize <= FloatSizeLimit)
						callingConv->ReturnPassway = ADR(REG, Platform::Bit64, BR(Registers::XMM0));
					else
						callingConv->ReturnPassway = ADR(MEM, Platform::Bit64, BR(Registers::XMM0));
					break;
				default:
					break;
				}
			}

			constexpr std::array<UInt8, 4> IntRegisters[] = {
				Register::X86::RegisterSet<Platform::Bit64>::CX,
				Register::X86::RegisterSet<Platform::Bit64>::DX,
				Register::X86::RegisterSet<Platform::Bit64>::R8,
				Register::X86::RegisterSet<Platform::Bit64>::R9
			};

			constexpr std::array<UInt8, 4> FloatRegisters = {
				Register::X86::RegisterSet<Platform::Bit64>::XMM0,
				Register::X86::RegisterSet<Platform::Bit64>::XMM1,
				Register::X86::RegisterSet<Platform::Bit64>::XMM2,
				Register::X86::RegisterSet<Platform::Bit64>::XMM3
			};

			Int32 intRegisterUsed = 0;
			Int32 floatRegisterUsed = 0;

			for (PlatformCallingArgument arg : range | std::views::drop(1))
			{
				switch (arg.Type)
				{
				case CallingArgumentType::Integer:
				{
					
					if (arg.LayoutSize <= IntegerSizeLimit)
						callingConv->ReturnPassway = ADR(REG, Platform::Bit64, BR(Registers::AX));
					else
						callingConv->ReturnPassway = ADR(REG | MEM, Platform::Bit64, BR(Registers::AX));
					break;
				}
				case CallingArgumentType::Float:
					if (arg.LayoutSize <= FloatSizeLimit)
						callingConv->ReturnPassway = ADR(REG, Platform::Bit64, BR(Registers::XMM0));
					else
						callingConv->ReturnPassway = ADR(MEM, Platform::Bit64, BR(Registers::XMM0));
					break;
				default:
					break;
				}
			}
		}
	};
}