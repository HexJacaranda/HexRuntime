#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"

namespace RTP
{
	class Platform
	{
	public:
		ETY = UInt32;

		//Architecture
		VAL AnyArchitecture = 0x00000000;
		VAL x86 = 0x00000001;
		VAL Arm = 0x00000002;

		//Width
		VAL AnyWidth   = 0x00000000;
		VAL Bit32 = 0x00010000;
		VAL Bit64 = 0x00020000;

		//OS
		VAL AnyOS = 0x00000000;
		VAL Windows = 0x00100000;		
	};

#ifdef LINUX
	constexpr UInt32 CurrentOS = Platform::AnyOS;
#define PLATFORM_LINUX
#else
	constexpr UInt32 CurrentOS = Platform::Windows;
#define PLATFORM_WINDOWS
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

#define PLATFORM_TEMPLATE_ARGS UInt32 Architecture, UInt32 Width, UInt32 OS
#define USE_CURRENT_PLATFORM CurrentArchitecture, CurrentWidth, CurrentOS

}