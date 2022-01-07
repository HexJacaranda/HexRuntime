#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\ObservableArray.h"
#include "PlatformSpecialization.h"

namespace RTP
{
	template<UInt32 OS>
	class PlatformCallingConventionProvider<CallingConventions::JIT, Platform::x86, Platform::Bit64, OS>
	{
	public:
		template<class RangeT>
		static PlatformCallingConvention* GetConvention(PlatformCallingConvention* callingConv, RangeT&& range)
		{
			return callingConv;
		}
	};

	template<UInt32 OS>
	class PlatformCallingConventionProvider<CallingConventions::JIT, Platform::x86, Platform::Bit32, OS>
	{
	public:
		template<class RangeT>
		static PlatformCallingConvention* GetConvention(PlatformCallingConvention* callingConv, RangeT&& range)
		{
			return callingConv;
		}
	};
}