#include "ImportSession.h"

RTM::ImportSession::ImportSession(MDPrivateHeap* heap, RTI::FileHandle handle):
	IImportSession(heap)
{
	mHandle = RTI::OSFile::Duplicate(handle);
}

RT::Int32 RTM::ImportSession::ReadInto(UInt8* memory, Int32 size)
{
	return RTI::OSFile::ReadInto(mHandle, memory, size);
}

void RTM::ImportSession::Relocate(Int32 offset, RTI::LocateOption option)
{
	return RTI::OSFile::Locate(mHandle, offset, option);
}

RTM::ImportSession::~ImportSession()
{
	RTI::OSFile::Close(mHandle);
}
