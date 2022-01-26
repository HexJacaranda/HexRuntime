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
		RTMM::PrivateHeap* mHeap;
		using RegSet = Register::X86::RegisterSet<Platform::Bit64>;
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