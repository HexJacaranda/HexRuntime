#pragma once
#include "..\RuntimeAlias.h"
namespace RTC
{
	class Type;
}
namespace RTC
{
	struct MethodSlot
	{
		UInt8 CallingConvention;
		UInt8 SlotType;
		UInt16 Flags;
		UInt8* Entry;
	};

	struct MethodSlotBundle
	{
		constexpr static Int32 SlotCount = 4;
		MethodSlot Slots[SlotCount];
	};

	class MethodTable
	{
		Type* mType;
	public:
		inline Type* GetOwningType()const;
		inline MethodSlotBundle* GetStartBundle()const;
		inline MethodSlot* GetSlotAt(Int32 index)const;
	};
}