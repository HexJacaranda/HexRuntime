#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"
#include "..\Type\TypeRepresentation.h"
#include "..\Meta\MethodDescriptor.h"
#include <vector>

namespace RTJ
{
	struct JITContext
	{
	public:
		class Performance
		{
		public:
			static constexpr UInt32 AggressiveInline = 0x00000001;
		};
		/// <summary>
		/// Performance settings
		/// </summary>
		UInt32 PerformanceSettings;
		/// <summary>
		/// Multiple tier compilation
		/// </summary>
		UInt16 TierSettings;

		RTM::MethodDescriptor* MethDescriptor;
	};
}