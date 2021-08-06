#include "PrivateHeap.h"

void RTMM::PrivateHeap::HangHeap(RawHeap heap)
{
	auto link = new HeapLink{ heap, nullptr };
	link->Previous = mCurrent.load(std::memory_order::relaxed);

	while (!mCurrent.compare_exchange_weak(link->Previous, link, std::memory_order::release, std::memory_order::relaxed));
}

RTMM::RawHeap RTMM::PrivateHeap::GetCurrentRawHeap()
{
	thread_local static RawHeap heap;
	if (heap == nullptr)
	{
		heap = mi_heap_new();
		HangHeap(heap);
	}
	return heap;
}

RTMM::PrivateHeap::~PrivateHeap()
{
	HeapLink* next = nullptr;
	HeapLink* iterator = mCurrent;
	while (iterator != nullptr)
	{
		next = iterator->Previous;
		mi_heap_destroy(iterator->Heap);
		delete iterator;
		iterator = next;
	}
}

void* RTMM::PrivateHeap::Allocate(Int32 size)
{
	return mi_heap_malloc(GetCurrentRawHeap(), size);
}

void* RTMM::PrivateHeap::Allocate(Int32 size, Int32 align)
{
	return mi_heap_malloc_aligned(GetCurrentRawHeap(), size, align);
}

void* RTMM::PrivateHeap::Free(void* target)
{
	mi_free(target);
}

void* operator new(size_t size, RTMM::PrivateHeap* allocator)
{
	return allocator->Allocate(size);
}

void* operator new[](size_t size, RTMM::PrivateHeap* allocator)
{
	return allocator->Allocate(size);
}

void operator delete(void* target, RTMM::PrivateHeap* allocator)
{
	allocator->Free(target);
}

void operator delete[](void* target, RTMM::PrivateHeap* allocator)
{
	allocator->Free(target);
}
