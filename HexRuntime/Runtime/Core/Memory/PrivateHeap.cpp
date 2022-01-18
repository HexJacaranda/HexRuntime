#include "PrivateHeap.h"

RTMM::PrivateHeap::~PrivateHeap()
{

}

RTMM::PrivateHeap::PrivateHeap()
{
	mResource = std::make_unique<std::pmr::synchronized_pool_resource>();
	mAllocator = std::make_unique<std::pmr::polymorphic_allocator<std::byte>>(mResource.get());
}

void* RTMM::PrivateHeap::Allocate(Int32 size)
{
	return mAllocator->allocate_bytes(size);
}

void* RTMM::PrivateHeap::Allocate(Int32 size, Int32 align)
{
	return mAllocator->allocate_bytes(size, align);
}

void RTMM::PrivateHeap::Free(void* target, Int32 size)
{
	return mAllocator->deallocate_bytes(target, size);
}

void RTMM::PrivateHeap::FreeTracked(void* target)
{
	Int32* origin = (Int32*)target - 1;
	mAllocator->deallocate_bytes(origin, *origin);
}

void* operator new(size_t size, RTMM::PrivateHeap* allocator)
{
	return allocator->Allocate(size);
}

void* operator new(size_t size, RTMM::PrivateHeap* allocator, RTMM::TrackTagT)
{
	void* memory = allocator->Allocate(size + sizeof(RT::Int32));
	*((RT::Int32*)memory) = size + sizeof(RT::Int32);
	return (RT::Int32*)memory + 1;
}

void* operator new[](size_t size, RTMM::PrivateHeap* allocator)
{
	return allocator->Allocate(size);
}

void* operator new[](size_t size, RTMM::PrivateHeap* allocator, RTMM::TrackTagT)
{
	void* memory = allocator->Allocate(size + sizeof(RT::Int32));
	*((RT::Int32*)memory) = size + sizeof(RT::Int32);
	return (RT::Int32*)memory + 1;
}

RTMM::PrivateHeap* RTMM::GlobalHeap = nullptr;
