#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Utility.h"
#include "..\Type\TypeRepresentation.h"
#include <vector>

namespace RTJ
{
	struct LocalVariableInfo
	{
		TypeRepresentation Type;
	};

	struct ArgumentInfo
	{
		TypeRepresentation Type;
		UInt64 DefaultValue;
	};

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
		/// <summary>
		/// OpCode segment
		/// </summary>
		UInt8* CodeSegment;
		/// <summary>
		/// Segment length
		/// </summary>
		Int32 SegmentLength;
		/// <summary>
		/// Local variable
		/// </summary>
		std::vector<LocalVariableInfo> LocalVariables;
		/// <summary>
		/// Arguments
		/// </summary>
		std::vector<ArgumentInfo> Arguments;
	};
}