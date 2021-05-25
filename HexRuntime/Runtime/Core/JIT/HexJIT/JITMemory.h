#pragma once
#include "..\..\..\RuntimeAlias.h"
#pragma warning(disable:4291)

namespace RTJ::Hex
{
	/// <summary>
	/// Provide an arena memory allocator
	/// </summary>
	class JITMemory
	{
		static constexpr Int32 SegmentSpace = 8192;
		struct Segment
		{
			Segment* Previous;
		};
		Segment* mOversizeSegmentCurrent = nullptr;
		Segment* mSegmentCurrent = nullptr;
		Int8* mCurrent = nullptr;
		void AllocateSegment();
		void AllocateOversizeSegment(Int32 size);
		bool IsMoreSegmentNeeded(Int32 size);
	public:
		JITMemory();
		~JITMemory();
		void* Allocate(Int32 size);
		template<class T>
		T* Allocate()
		{
			return (T*)Allocate(sizeof(T));
		}
	};
}

/// <summary>
/// Overload new for arena memory allocator
/// </summary>
/// <param name="size"></param>
/// <param name="allocator"></param>
/// <returns></returns>
void* operator new(size_t size, RTJ::Hex::JITMemory* allocator);

void* operator new[](size_t size, RTJ::Hex::JITMemory* allocator);