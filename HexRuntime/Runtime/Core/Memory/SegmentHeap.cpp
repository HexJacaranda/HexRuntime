#include "SegmentHeap.h"
#include "..\Exception\RuntimeException.h"

RTMM::SegmentHeap::SegmentHeap()
{
	mAllocator = std::make_unique<std::pmr::polymorphic_allocator<std::byte>>();
}

RTMM::SegmentHeap::~SegmentHeap()
{
	for (auto&& point : mCollectPoints)
		point.Method(point.Address);
}

void* RTMM::SegmentHeap::Allocate(size_t bytes)
{
	return mAllocator->allocate_bytes(bytes, Align);
}

void* operator new(size_t size, RTMM::SegmentHeap* allocator)
{
	return allocator->Allocate(size);
}

void* operator new[](size_t size, RTMM::SegmentHeap* allocator)
{
	return allocator->Allocate(size);
}

void operator delete(void* target, RTMM::SegmentHeap* allocator)
{
}

void operator delete[](void* target, RTMM::SegmentHeap* allocator)
{
}
