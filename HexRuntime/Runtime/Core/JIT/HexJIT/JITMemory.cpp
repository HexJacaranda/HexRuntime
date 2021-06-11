#include "JITMemory.h"
#include "..\..\Exception\RuntimeException.h"
#include <memory>

void RTJ::Hex::JITMemory::AllocateSegment()
{
	Segment* newOne = (Segment*)std::malloc(SegmentSpace + sizeof(Segment));
	if (newOne == nullptr)
		RTE::Throw(Text("Failed to allocate more space for JIT"));
	else
	{
		newOne->Previous = mPODCurrent;;
		mPODCurrent = newOne;
		mPODStart = (Int8*)(mPODCurrent + 1);
	}
}

void RTJ::Hex::JITMemory::AllocateNonPODSegment()
{
	Segment* newOne = (Segment*)std::malloc(SegmentSpace + sizeof(Segment));
	if (newOne == nullptr)
		RTE::Throw(Text("Failed to allocate more space for JIT"));
	else
	{
		newOne->Previous = mNonPODCurrent;;
		mNonPODCurrent = newOne;
		mNonPODStart = (Int8*)(mNonPODCurrent + 1);
	}
}

void RTJ::Hex::JITMemory::AllocateOversizeSegment(Int32 size)
{
	Segment* newOne = (Segment*)std::malloc(size + sizeof(Segment));
	if (newOne == nullptr)
		RTE::Throw(Text("Failed to allocate more space for JIT"));
	else
	{
		newOne->Previous = mOversizeCurrent;
		mOversizeCurrent = newOne;
	}
}

bool RTJ::Hex::JITMemory::IsMoreSegmentNeeded(Int32 size)
{
	Int8* endAddress = (Int8*)(mPODCurrent + 1) + SegmentSpace;
	return endAddress - mPODStart < size;
}

bool RTJ::Hex::JITMemory::IsMoreNonPODSegmentNeeded(Int32 size)
{
	Int8* endAddress = (Int8*)(mNonPODCurrent + 1) + SegmentSpace;
	return endAddress - mNonPODStart < size + 2 * sizeof(NonPODEntry);
}

void RTJ::Hex::JITMemory::MarkNonPODSegmentEnd()
{
	//Set ending NonPOD entry info
	NonPODEntry* endMark = (NonPODEntry*)mNonPODStart;
	endMark->Size = 0;
}

void RTJ::Hex::JITMemory::ReleaseOversizeArea()
{
	Segment* next = nullptr;
	while (mOversizeCurrent != nullptr)
	{
		next = mOversizeCurrent->Previous;
		std::free(mOversizeCurrent);
		mOversizeCurrent = next;
	}
}

void RTJ::Hex::JITMemory::ReleaseNonPODArea()
{
	Segment* next = nullptr;
	while (mNonPODCurrent != nullptr)
	{
		NonPODEntry* entry = (NonPODEntry*)(mNonPODCurrent + 1);
		NonPODEntry* upperBound = (NonPODEntry*)((Int8*)(mNonPODCurrent + 1) + SegmentSpace);

		while (entry->Size != 0)
		{
			auto userData = entry + 1;
			entry->ReleaseCall(userData);
			entry = (NonPODEntry*)((Int8*)userData + entry->Size);
		}

		next = mNonPODCurrent->Previous;
		std::free(mNonPODCurrent);
		mNonPODCurrent = next;
	}
}

void RTJ::Hex::JITMemory::ReleasePODArea()
{
	Segment* next = nullptr;
	while (mPODCurrent != nullptr)
	{
		next = mPODCurrent->Previous;
		std::free(mPODCurrent);
		mPODCurrent = next;
	}
}

RTJ::Hex::JITMemory::JITMemory()
{
	AllocateSegment();
	AllocateNonPODSegment();
	MarkNonPODSegmentEnd();
}

RTJ::Hex::JITMemory::~JITMemory()
{
	ReleaseNonPODArea();
	ReleasePODArea();
	ReleaseOversizeArea();
}

void* RTJ::Hex::JITMemory::AllocateNonPOD(Int32 size, NonPODReleaseCall call)
{
	if (size > SegmentSpace)
	{
		//Unsupported now
	}
	if (IsMoreNonPODSegmentNeeded(size))
		AllocateNonPODSegment();
	
	//Set NonPOD entry info
	NonPODEntry* entry = (NonPODEntry*)mNonPODStart;
	entry->Size = size;
	entry->ReleaseCall = call;

	//Get space
	mNonPODStart += sizeof(NonPODEntry);
	Int8* space = (Int8*)mNonPODStart;
	mNonPODStart += size;

	MarkNonPODSegmentEnd();
	return space;
}

void* RTJ::Hex::JITMemory::Allocate(Int32 size)
{
	if (size > SegmentSpace)
	{
		AllocateOversizeSegment(size);
		return mOversizeCurrent + 1;
	}
	if (IsMoreSegmentNeeded(size))
		AllocateSegment();
	Int8* space = mPODStart;
	mPODStart += size;
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
