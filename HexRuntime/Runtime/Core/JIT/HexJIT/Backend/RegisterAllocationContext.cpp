#include "RegisterAllocationContext.h"
#include "..\..\..\..\Bit.h"

namespace RTJ::Hex
{
	std::optional<UInt8> RegisterAllocationContext::TryGetFreeRegister(UInt64 mask)
	{
		UInt8 bit = Bit::LeftMostSetBit(mRegisterPool & mask);
		if (bit != Bit::InvalidBit)
		{
			Bit::SetZero(mRegisterPool, bit);
			return bit;
		}
		else
			return {};
	}

	void RegisterAllocationContext::ReturnRegister(UInt8 nativeReg)
	{
		Bit::SetOne(mRegisterPool, nativeReg);
	}

	std::optional<UInt8> RegisterAllocationContext::TryGetRegister(UInt16 variable) const
	{
		auto regLocator = mVar2Reg.find(variable);
		if (regLocator != mVar2Reg.end())
			return regLocator->second;
		return {};
	}

	void RegisterAllocationContext::TryInvalidateFor(UInt16 variable)
	{
		auto regLocator = mVar2Reg.find(variable);
		if (regLocator != mVar2Reg.end())
		{
			//Set register free
			ReturnRegister(regLocator->second);
			mVar2Reg.erase(regLocator);
		}
	}

	void RegisterAllocationContext::TransferRegisterFor(UInt16 variable, UInt8 newRegister)
	{
		auto old = TryGetRegister(variable);
		if (old.has_value())
		{
			ReturnRegister(old.value());
			mVar2Reg[variable] = newRegister;
		}
	}

	void RegisterAllocationContext::Establish(UInt16 variable, UInt8 allocatedRegister)
	{
		mVar2Reg[variable] = allocatedRegister;
	}

	std::tuple<std::optional<UInt8>, bool>
		RegisterAllocationContext::AllocateRegisterFor(UInt16 variable, UInt64 mask) const
	{
		auto originReg = TryGetRegister(variable);
		if (originReg.has_value())
		{
			if (Bit::TestAt(mask, originReg.value()))
			{
				//Origin register satisfy the mask
				return { originReg, false };
			}
			else
			{
				//Return old, but need further operation to satisfy the mask
				return { originReg, true };
			}
		}
		else
		{
			//No register available
			return { {}, true };
		}
	}
	std::unordered_map<UInt16, UInt8> const& RegisterAllocationContext::GetMapping() const
	{
		return mVar2Reg;
	}
}