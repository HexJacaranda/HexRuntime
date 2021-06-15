#include "MDHeap.h"

RT::UInt8* RTM::MDPrivateHeap::Allocate(Int32 count)
{
	return nullptr;
}

RT::UInt8* RTM::MDPrivateHeap::AllocateForCode(Int32 count)
{
	return nullptr;
}

void RTM::MDPrivateHeap::Unload()
{
}

RTM::MDPrivateHeap* RTM::MDHeap::GetPrivateHeap(MDToken assemblyToken)
{
	return nullptr;
}
