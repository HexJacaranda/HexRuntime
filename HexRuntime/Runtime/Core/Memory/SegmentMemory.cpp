#include "SegmentMemory.h"
#include "..\Exception\RuntimeException.h"
#include <mimalloc.h>

void RTMM::SegmentMemory::AllocateSegment()
{
	Segment* newOne = (Segment*)mi_malloc(mSegmentSize + sizeof(Segment));
	if (newOne == nullptr)
		THROW("Failed to allocate more space.");
	else
	{
		newOne->Previous = mPODCurrent;;
		mPODCurrent = newOne;
		mPODStart = (Int8*)(mPODCurrent + 1);
	}
}

void RTMM::SegmentMemory::AllocateNonPODSegment()
{
	Segment* newOne = (Segment*)mi_malloc(mSegmentSize + sizeof(Segment));
	if (newOne == nullptr)
		THROW("Failed to allocate more space.");
	else
	{
		newOne->Previous = mNonPODCurrent;;
		mNonPODCurrent = newOne;
		mNonPODStart = (Int8*)(mNonPODCurrent + 1);
	}
}

void RTMM::SegmentMemory::AllocateOversizeSegment(Int32 size)
{
	Segment* newOne = (Segment*)mi_malloc(size + sizeof(Segment));
	if (newOne == nullptr)
		THROW("Failed to allocate more space.");
	else
	{
		newOne->Previous = mOversizeCurrent;
		mOversizeCurrent = newOne;
	}
}

bool RTMM::SegmentMemory::IsMoreSegmentNeeded(Int32 size)
{
	Int8* endAddress = (Int8*)(mPODCurrent + 1) + mSegmentSize;
	return endAddress - mPODStart < size;
}

bool RTMM::SegmentMemory::IsMoreNonPODSegmentNeeded(Int32 size)
{
	Int8* endAddress = (Int8*)(mNonPODCurrent + 1) + mSegmentSize;
	return endAddress - mNonPODStart < size + 2 * sizeof(NonPODEntry);
}

void RTMM::SegmentMemory::MarkNonPODSegmentEnd()
{
	//Set ending NonPOD entry info
	NonPODEntry* endMark = (NonPODEntry*)mNonPODStart;
	endMark->Size = 0;
}

void RTMM::SegmentMemory::ReleaseOversizeArea()
{
	Segment* next = nullptr;
	while (mOversizeCurrent != nullptr)
	{
		next = mOversizeCurrent->Previous;
		mi_free(mOversizeCurrent);
		mOversizeCurrent = next;
	}
}

void RTMM::SegmentMemory::ReleaseNonPODArea()
{
	Segment* next = nullptr;
	while (mNonPODCurrent != nullptr)
	{
		NonPODEntry* entry = (NonPODEntry*)(mNonPODCurrent + 1);
		NonPODEntry* upperBound = (NonPODEntry*)((Int8*)(mNonPODCurrent + 1) + mSegmentSize);

		while (entry->Size != 0)
		{
			auto userData = entry + 1;
			entry->Release(userData);
			entry = (NonPODEntry*)((Int8*)userData + entry->Size);
		}

		next = mNonPODCurrent->Previous;
		mi_free(mNonPODCurrent);
		mNonPODCurrent = next;
	}
}

void RTMM::SegmentMemory::ReleasePODArea()
{
	Segment* next = nullptr;
	while (mPODCurrent != nullptr)
	{
		next = mPODCurrent->Previous;
		mi_free(mPODCurrent);
		mPODCurrent = next;
	}
}

RTMM::SegmentMemory::SegmentMemory(Int32 align, Int32 segmentSize) :
	mAlign(align),
	mSegmentSize(segmentSize)
{
	AllocateSegment();
	AllocateNonPODSegment();
	MarkNonPODSegmentEnd();
}

RTMM::SegmentMemory::SegmentMemory() : SegmentMemory(Align, SegmentSize)
{
}

RTMM::SegmentMemory::~SegmentMemory()
{
	ReleaseNonPODArea();
	ReleasePODArea();
	ReleaseOversizeArea();
}

void* RTMM::SegmentMemory::AllocateNonPOD(Int32 size, ReleaseFun call)
{
	if (size > mSegmentSize)
	{
		//Unsupported now
	}
	if (IsMoreNonPODSegmentNeeded(size))
		AllocateNonPODSegment();

	//Set NonPOD entry info
	NonPODEntry* entry = (NonPODEntry*)mNonPODStart;
	entry->Size = size;
	entry->Release = call;

	//Get space
	mNonPODStart += sizeof(NonPODEntry);
	Int8* space = (Int8*)mNonPODStart;
	mNonPODStart += size;

	MarkNonPODSegmentEnd();
	return space;
}

void* RTMM::SegmentMemory::Allocate(Int32 size)
{
	if (size > mSegmentSize)
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

void* operator new(size_t size, RTMM::SegmentMemory* allocator)
{
	return allocator->Allocate(size);
}

void* operator new[](size_t size, RTMM::SegmentMemory* allocator)
{
	return allocator->Allocate(size);
}

void operator delete(void* target, RTMM::SegmentMemory* allocator)
{
}

void operator delete[](void* target, RTMM::SegmentMemory* allocator)
{
}
