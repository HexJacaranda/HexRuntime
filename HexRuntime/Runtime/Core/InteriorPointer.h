#pragma once
#include "..\RuntimeAlias.h"
#include "Object/Object.h"
namespace RTC
{
	template<int pointerSize>
	struct InteriorPointerBase;

	template<>
	struct InteriorPointerBase<4>
	{
		union {
			struct {
				RTO::ObjectRef BasePointer;
				Int Offset;
			};
			Int64 Value;
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
}