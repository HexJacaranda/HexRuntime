#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\ObservableArray.h"

namespace RTM
{
	class TypeDescriptor;
}

namespace RTEE
{

	struct StackFrameSlot
	{
		Int32 Offset;
		RTM::TypeDescriptor* Type;
	};

	/// <summary>
	/// Static information of a managed method
	/// </summary>
	struct StackFrameInfo
	{
		ObservableArray<StackFrameSlot> Variables;
	};

	enum class RuntimeStackFrameKinds
	{
		Managed,
		RuntimeNative,
		Native,
		Unkown
	};

	struct RuntimeStackFrame
	{
		Int8* StackPointer;
		RuntimeStackFrameKinds Kind;
		union 
		{
			StackFrameInfo* FrameInfo;
		};		
	};

	/// <summary>
	/// Clustering runtime stack frames to support fast allocation
	/// with simple operation: bumping pointer
	/// </summary>
	struct RuntimeStackFrameSegment
	{
		RuntimeStackFrame* Start;
		RuntimeStackFrame* Top;
		RuntimeStackFrame* Current;
		RuntimeStackFrameSegment* Previous;
	};

	template<class Fn>
	concept FrameActionT = requires(Fn fun, RuntimeStackFrame * &frame) { fun(frame); };

	template<FrameActionT Fn>
	static void IterateOverFrames(RuntimeStackFrameSegment* current, Fn&& action)
	{

	}
}