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
		PlatformCallingConventionProvider(RTMM::PrivateHeap* heap) {

		}
		template<class RangeT>
		PlatformCallingConvention* GetConvention(RangeT&& range)
		{
			return nullptr;
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
			return nullptr;
		}
	};
}