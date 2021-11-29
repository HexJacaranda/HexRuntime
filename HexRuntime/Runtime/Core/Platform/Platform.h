#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"

namespace RTP
{
	class Platform
	{
	public:
		UNDERLYING_TYPE(UInt32);

		//Architecture
		VALUE(AnyArchitecture) = 0x00000000;
		VALUE(x86) = 0x00000001;
		VALUE(Arm) = 0x00000002;

		//Width
		VALUE(AnyWidth)   = 0x00000000;
		VALUE(Bit32) = 0x00010000;
		VALUE(Bit64) = 0x00020000;

		//OS
		VALUE(AnyOS) = 0x00000000;
		VALUE(Windows) = 0x00100000;		
	};

	
#ifdef LINUX
	constexpr UInt32 CurrentOS = Platform::AnyOS;
#else
	constexpr UInt32 CurrentOS = Platform::Windows;
#endif

#ifdef ARM
	constexpr UInt32 CurrentArchitecture = Platform::Arm;
#else
	constexpr UInt32 CurrentArchitecture = Platform::x86;
#endif
	constexpr UInt32 CurrentWidth = sizeof(void*) == 4 ? Platform::Bit32 : Platform::Bit64;

	constexpr UInt32 CurrentPlatform = CurrentArchitecture | CurrentWidth | CurrentOS;

	enum class CallingConventions
	{
		JIT,
		Cdecl,
		Std,
		Fast,
		Vector
	};
}