#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"
#include "..\Memory\PrivateHeap.h"
#include "Platform.h"
#include <array>
#include <ranges>

namespace RTP
{
	struct AddressConstraintFlags
	{
	public:
		ETY = UInt16;
		VAL Invalid = 0x0000;
		VAL Memory = 0x0001;
		VAL Register = 0x0002;
	};

	struct AddressConstraint
	{
		AddressConstraint(UInt8 single, UInt16 flags) :
			SingleRegister(single),
			Flags(flags) {}
		AddressConstraint(UInt16 flags) :
			Flags(flags) {}
		AddressConstraint() {}

		UInt8 SingleRegister = 0;
		UInt16 Flags = 0;

		bool HasMemory()const {
			return Flags & AddressConstraintFlags::Memory;
		}
		bool HasRegister()const {
			return Flags & AddressConstraintFlags::Register;
		}
	};

	struct RegisterStateFlags
	{
		ETY = UInt16;
		VAL Volatile = 0b0;
		VAL NonVolatile = 0b1;

		VAL Whole = 0b000;
		VAL LowerHalf = 0b100;
		VAL UpperHalf = 0b010;
	};

#define RVOL_F RTP::RegisterStateFlags::Volatile
#define RNVOL_F RTP::RegisterStateFlags::NonVolatile
#define RWHO_F RTP::RegisterStateFlags::Whole

	using RegisterState = UInt16;

	struct PlatformCallingConvention
	{ 
		Int32 ArgumentCount;
		/* For address constraint here, theoretically only SREG and MEM can be supported by common platforms. 
		* Specially, SREG | MEM indicates that it's placed on stack and address passed in register (See some 
		* calling convetions in X86-64)
		*/
		AddressConstraint* ArgumentPassway;
		AddressConstraint ReturnPassway;
		/* Should be accessed like RegisterStates[Register] */
		const RegisterState* RegisterStates;
	};
	
	class CallingArgumentType
	{
	public: 
		ETY = UInt32;

		VAL Integer = 0x00000000;
		VAL Float = 0x00000001;
		VAL Struct = 0x00000002;
	};

	struct PlatformCallingArgument
	{
		Int32 LayoutSize = 0;
		UInt32 Type = CallingArgumentType::Integer;
	};

	template<CallingConventions convention, PLATFORM_TEMPLATE_ARGS>
	class PlatformCallingConventionProvider
	{
	public:
		PlatformCallingConventionProvider(RTMM::PrivateHeap* heap) {

		}
		template<class RangeT>
		PlatformCallingConvention* GetConvention(RangeT&&)
		{
			return nullptr;
		}
	};
}
