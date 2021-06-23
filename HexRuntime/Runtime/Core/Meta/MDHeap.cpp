#include "MDHeap.h"

RT::UInt8* RTME::MDPrivateHeap::Allocate(Int32 count)
{
	return nullptr;
}

RT::UInt8* RTME::MDPrivateHeap::AllocateForCode(Int32 count)
{
	return nullptr;
}

void RTME::MDPrivateHeap::Unload()
{
}

RTME::MDPrivateHeap* RTME::MDHeap::GetPrivateHeap(MDToken assemblyToken)
{
	return nullptr;
}

void* operator new(size_t size, RTME::MDPrivateHeap* heap)
{
	return heap->Allocate(size);
}
