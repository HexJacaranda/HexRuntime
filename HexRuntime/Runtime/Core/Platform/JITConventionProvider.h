#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\ObservableArray.h"
#include "PlatformSpecialization.h"

namespace RTP
{
	template<UInt32 platform>
	class PlatformCallingConventionProvider<CallingConventions::JIT, platform>
	{
	public:
		template<class RangeT>
		static PlatformCallingConvention* GetConvention(PlatformCallingConvention* callingConv, RangeT&& range)
		{
			//Utilize more registers 64 bit
			if constexpr (platform & Platform::Bit32)
			{

			}
			else
			{

			}

			return callingConv;
		}
	};
}