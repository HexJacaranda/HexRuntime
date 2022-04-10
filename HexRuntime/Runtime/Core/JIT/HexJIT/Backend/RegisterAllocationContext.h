#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include <unordered_map>
#include <optional>
#include <tuple>

namespace RTJ::Hex
{
	class RegisterAllocationContext
	{
		std::unordered_map<UInt16, UInt8> mVar2Reg;
		UInt64 mRegisterPool = 0xFFFFFFFFFFFFFFFFull;
		UInt64 mUseMask = 0ull;
	public:
		std::optional<UInt8> TryGetFreeRegister(UInt64 mask);
		void ReturnRegister(UInt8 nativeReg);
		std::optional<UInt8> TryGetRegister(UInt16 variable) const;
		void TryInvalidateFor(UInt16 variable);
		void TransferRegisterFor(UInt16 variable, UInt8 newRegister);
		void Establish(UInt16 variable, UInt8 allocatedRegister);
		bool CanAllocateFor(UInt64 mask) const;
		std::tuple<std::optional<UInt8>, bool> AllocateRegisterFor(UInt16 variable, UInt64 mask) const;
		std::unordered_map<UInt16, UInt8> const& GetMapping()const;
		UInt64 GetUsedRegisterRecord()const;
	};
}