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
		/// <summary>
		/// Flatten model, content is laid out in-place
		/// </summary>
		VAL AsStruct = 0x0000;

		/// <summary>
		/// Indirect model, content is accessed by address
		/// </summary>
		VAL AsRef = 0x0010;
		VAL SemanticMask = 0x00F0;

		VAL Register = 0x0000;
		VAL Stack = 0x0001;
		VAL PlaceMask = 0x000F;

		VAL Invalid = 0xFFFF;
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

		bool InRegister()const {
			return (Flags & AddressConstraintFlags::PlaceMask) == AddressConstraintFlags::Register;
		}
		bool InStack()const {
			return (Flags & AddressConstraintFlags::PlaceMask) == AddressConstraintFlags::Register;
		}
		bool IsStructLike()const {
			return (Flags & AddressConstraintFlags::SemanticMask) == AddressConstraintFlags::AsStruct;
		}
		bool IsRefLike()const {
			return (Flags & AddressConstraintFlags::SemanticMask) == AddressConstraintFlags::AsRef;
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
