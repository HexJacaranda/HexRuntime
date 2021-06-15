#pragma once
#include "..\RuntimeAlias.h"
#include "Object/Object.h"
namespace RTC
{
	template<std::size_t pointerSize>
	struct InteriorPointerBase;

	template<>
	struct InteriorPointerBase<4>
	{
		struct {
			RTO::ObjectRef BasePointer;
			Int Offset;
		};
	};

	template<>
	struct InteriorPointerBase<8>
	{
		struct alignas(16) {
			RTO::ObjectRef BasePointer;
			Int Offset;
		};
	};

	using InteriorPointer = InteriorPointerBase<sizeof(void*)>;

	//Double word compare and swap
	extern "C" bool __stdcall DWCAS(InteriorPointer * address, InteriorPointer value, InteriorPointer compareValue);
}