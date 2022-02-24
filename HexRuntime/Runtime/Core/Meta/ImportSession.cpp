#include "ImportSession.h"

RTME::ImportSession::ImportSession(RTMM::PrivateHeap* heap, RTI::FileHandle handle):
	IImportSession(heap)
{
	mHandle = RTI::OSFile::Duplicate(handle);
}

RT::Int32 RTME::ImportSession::ReadInto(UInt8* memory, Int32 size)
{
	return RTI::OSFile::ReadInto(mHandle, memory, size);
}

void RTME::ImportSession::Relocate(Int32 offset, RTI::LocateOption option)
{
	return RTI::OSFile::Locate(mHandle, offset, option);
}

RT::Int32 RTME::ImportSession::GetCurrentPosition() const
{
	return RTI::OSFile::GetCurrentLocation(mHandle);
}

RTME::ImportSession::~ImportSession()
{
	RTI::OSFile::Close(mHandle);
}
