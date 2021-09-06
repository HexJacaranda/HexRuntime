#pragma once
#include "..\..\RuntimeAlias.h"
#include <mimalloc.h>
#include <atomic>

namespace RTMM
{
	using RawHeap = mi_heap_t*;

	struct HeapLink
	{
		RawHeap Heap;
		HeapLink* Previous;
	};

	class PrivateHeap
	{
		std::atomic<HeapLink*> mCurrent = nullptr;

		void HangHeap(RawHeap heap);
		/// <summary>
		/// Thread-local static heap for every thread
		/// </summary>
		/// <returns></returns>
		RawHeap GetCurrentRawHeap();
	public:
		~PrivateHeap();

		void* Allocate(Int32 size);
		void* Allocate(Int32 size, Int32 align);
		void Free(void* target);
	};
}

void* operator new(size_t size, RTMM::PrivateHeap* allocator);

void* operator new[](size_t size, RTMM::PrivateHeap* allocator);

void operator delete(void* target, RTMM::PrivateHeap* allocator);

void operator delete[](void* target, RTMM::PrivateHeap* allocator);
