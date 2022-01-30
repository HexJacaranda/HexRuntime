#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\ObservableArray.h"
#include "PlatformSpecialization.h"
#include "..\Platform\X86Register.h"

namespace RTP
{
	template<UInt32 OS>
	class PlatformCallingConventionProvider<CallingConventions::JIT, Platform::x86, Platform::Bit64, OS>
	{
		using RegSet = Register::X86::RegisterSet<Platform::Bit64>;
		/// <summary>
		/// TODO: Currently use X64 calling convention
		/// </summary>
		static constexpr std::array<RegisterState, RegSet::RegisterCount> States =
		{
			RVOL_F | RWHO_F, //REG AX = 0x00;
			RVOL_F | RWHO_F, //REG CX = 0x01;
			RVOL_F | RWHO_F, //REG DX = 0x02;
			RNVOL_F | RWHO_F, //REG BX = 0x03;
			RNVOL_F | RWHO_F, //REG SP = 0x04;
			RNVOL_F | RWHO_F, //REG BP = 0x05;
			RNVOL_F | RWHO_F, //REG SI = 0x06;
			RNVOL_F | RWHO_F, //REG DI = 0x07;
			RVOL_F | RWHO_F,//REG R8 = 0x08;
			RVOL_F | RWHO_F,//REG R9 = 0x09;
			RVOL_F | RWHO_F,//REG R10 = 0x0A;
			RVOL_F | RWHO_F,//REG R11 = 0x0B;
			RNVOL_F | RWHO_F,//REG R12 = 0x0C;
			RNVOL_F | RWHO_F,//REG R13 = 0x0D;
			RNVOL_F | RWHO_F,//REG R14 = 0x0E;
			RNVOL_F | RWHO_F,//REG R15 = 0x0F;

			RVOL_F | RWHO_F,//REG XMM0 = 0X10;
			RVOL_F | RWHO_F,//REG XMM1 = 0X11;
			RVOL_F | RWHO_F,//REG XMM2 = 0X12;
			RVOL_F | RWHO_F,//REG XMM3 = 0X13;
			RVOL_F | RWHO_F,//REG XMM4 = 0X14;
			RVOL_F | RWHO_F,//REG XMM5 = 0X15;
			RNVOL_F | RWHO_F,//REG XMM6 = 0X16;
			RNVOL_F | RWHO_F,//REG XMM7 = 0X17;
			RNVOL_F | RWHO_F,//REG XMM8 = 0X18;
			RNVOL_F | RWHO_F,//REG XMM9 = 0X19;
			RNVOL_F | RWHO_F,//REG XMM10 = 0X1A;
			RNVOL_F | RWHO_F,//REG XMM11 = 0X1B;
			RNVOL_F | RWHO_F,//REG XMM12 = 0X1C;
			RNVOL_F | RWHO_F,//REG XMM13 = 0X1D;
			RNVOL_F | RWHO_F,//REG XMM14 = 0X1E;
			RNVOL_F | RWHO_F //REG XMM15 = 0X1F;
		};

		RTMM::PrivateHeap* mHeap;		
	public:
		PlatformCallingConventionProvider(RTMM::PrivateHeap* heap) :
			mHeap(heap) {}

		template<class RangeT>
		PlatformCallingConvention* GetConvention(RangeT&& range)
		{
			constexpr std::array<UInt8, 4> iRegisters = {
				RegSet::CX,
				RegSet::DX,
				RegSet::R8,
				RegSet::R9
			};

			constexpr std::array<UInt8, 4> fRegisters = {
				RegSet::XMM0,
				RegSet::XMM1,
				RegSet::XMM2,
				RegSet::XMM3
			};

			Int32 iRegIndex = 0;
			Int32 fRegIndex = 0;

			auto allocateSpace = [&](PlatformCallingArgument const& arg) -> AddressConstraint {
				switch (arg.Type)
				{
				case CallingArgumentType::Float:
				{
					if (fRegIndex < fRegisters.size())
						return AddressConstraint(fRegisters[fRegIndex++], AddressConstraintFlags::Register);
					return AddressConstraint(AddressConstraintFlags::Memory);
				}
				case CallingArgumentType::Integer:
				{
					if (iRegIndex < iRegisters.size())
						return AddressConstraint(iRegisters[iRegIndex++], AddressConstraintFlags::Register);
					return AddressConstraint(AddressConstraintFlags::Memory);
				}
				case CallingArgumentType::Struct:
				{
					if (iRegIndex < iRegisters.size())
						return AddressConstraint(iRegisters[iRegIndex++],
							AddressConstraintFlags::Register | AddressConstraintFlags::Memory);
					return AddressConstraint(AddressConstraintFlags::Memory);
				}
				default:
					return AddressConstraint(AddressConstraintFlags::Invalid);
				}
			};

			auto callingConv = new (mHeap) PlatformCallingConvention;
			callingConv->ArgumentPassway = new (mHeap) AddressConstraint[range.size() - 1];

			PlatformCallingArgument returnArg = range.front();
			AddressConstraint returnCons{};
			if (returnArg.LayoutSize == 0)
				callingConv->ReturnPassway = AddressConstraint(AddressConstraintFlags::Invalid);
			else
			{
				switch (returnArg.Type)
				{
				case CallingArgumentType::Float:
					returnCons = AddressConstraint(RegSet::XMM0, AddressConstraintFlags::Register);
					break;
				case CallingArgumentType::Integer:
					returnCons = AddressConstraint(RegSet::AX, AddressConstraintFlags::Register);
					break;
				case CallingArgumentType::Struct:
					returnCons = AddressConstraint(RegSet::AX,
						AddressConstraintFlags::Register | AddressConstraintFlags::Memory);
					break;
				}
			}
			callingConv->ReturnPassway = returnCons;

			//Handle argument
			Int32 index = 0;
			for (auto&& arg : range | std::views::drop(1))
			{
				callingConv->ArgumentPassway[index] = allocateSpace(arg);
				index++;
			}
			callingConv->ArgumentCount = index;
			callingConv->RegisterStates = States.data();
			return callingConv;
		}
	};

	template<UInt32 OS>
	class PlatformCallingConventionProvider<CallingConventions::JIT, Platform::x86, Platform::Bit32, OS>
	{
	public:
		PlatformCallingConventionProvider(RTMM::PrivateHeap* heap) {

		}

		template<class RangeT>
		PlatformCallingConvention* GetConvention(RangeT&& range)
		{

		}
	};
}