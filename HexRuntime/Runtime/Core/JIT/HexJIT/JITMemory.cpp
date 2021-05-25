#include "JITMemory.h"
#include "..\..\Exception\RuntimeException.h"
#include <memory>

void RTJ::Hex::JITMemory::AllocateSegment()
{
	Segment* newOne = (Segment*)std::malloc(SegmentSpace + sizeof(Segment));
	if (newOne == nullptr)
		RTE::Throw(Text("Failed to allocate more space for JIT"));
	newOne->Previous = mSegmentCurrent;
	mSegmentCurrent = newOne;
	mCurrent = (Int8*)(mSegmentCurrent + 1);
}

void RTJ::Hex::JITMemory::AllocateOversizeSegment(Int32 size)
{
	Segment* newOne = (Segment*)std::malloc(size + sizeof(Segment));
	if (newOne == nullptr)
		RTE::Throw(Text("Failed to allocate more space for JIT"));
	newOne->Previous = mOversizeSegmentCurrent;
	mOversizeSegmentCurrent = newOne;
}

bool RTJ::Hex::JITMemory::IsMoreSegmentNeeded(Int32 size)
{
	Int8* startAddress = (Int8*)(mCurrent + 1);
	return mCurrent - startAddress < size;
}

RTJ::Hex::JITMemory::JITMemory()
{
	AllocateSegment();
}

RTJ::Hex::JITMemory::~JITMemory()
{
	Segment* next = nullptr;
	while (mSegmentCurrent != nullptr)
	{
		next = mSegmentCurrent->Previous;
		std::free(mSegmentCurrent);
		mSegmentCurrent = next;
	}
	while (mOversizeSegmentCurrent != nullptr)
	{
		next = mOversizeSegmentCurrent->Previous;
		std::free(mOversizeSegmentCurrent);
		mOversizeSegmentCurrent = next;
	}
}

void* RTJ::Hex::JITMemory::Allocate(Int32 size)
{
	if (size > SegmentSpace)
	{
		AllocateOversizeSegment(size);
		return mOversizeSegmentCurrent;
	}
	if (IsMoreSegmentNeeded(size))
		AllocateSegment();
	Int8* space = mCurrent;
	mCurrent += size;
	return space;
}

void* operator new(size_t size, RTJ::Hex::JITMemory* allocator)
{
	return allocator->Allocate(size);
}

void* operator new[](size_t size, RTJ::Hex::JITMemory* allocator)
{
	return allocator->Allocate(size);
}
