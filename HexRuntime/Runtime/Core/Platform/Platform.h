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
		VAL ArchitectureMask = 0x0000000FF;

		//Width
		VAL AnyWidth   = 0x00000000;
		VAL Bit32 = 0x00010000;
		VAL Bit64 = 0x00020000;
		VAL WidthMask = 0x000F0000;

		//OS
		VAL AnyOS = 0x00000000;
		VAL Windows = 0x00100000;		
		VAL OSMask = 0x00F00000;
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