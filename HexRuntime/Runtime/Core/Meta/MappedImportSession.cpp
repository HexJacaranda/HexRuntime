#include "MappedImportSession.h"
#include <memory>

RTME::MappedImportSession::MappedImportSession(RTMM::PrivateHeap* heap, UInt8* address, Int32 fileSize) :
	IImportSession(heap),
	mFileSize(fileSize),
	mMappedBase(address),
	mCurrent(address)
{

}

RT::Int32 RTME::MappedImportSession::ReadInto(UInt8* memory, Int32 size)
{
	Int32 toRead = size;
	if (mFileSize - (mCurrent - mMappedBase) < size)
		toRead = mFileSize - (mCurrent - mMappedBase);
	std::memmove(memory, mCurrent, size);
	mCurrent += toRead;
	return toRead;
}

void RTME::MappedImportSession::Relocate(Int32 offset, RTI::LocateOption option)
{
	switch (option)
	{
	case Runtime::Core::Interfaces::LocateOption::Start:
		if (offset >= 0 && offset <= mFileSize)
			mCurrent = mMappedBase + offset;
		break;
	case Runtime::Core::Interfaces::LocateOption::Current:
		if (mCurrent + offset < mMappedBase + mFileSize)
			mCurrent += offset;
		break;
	case Runtime::Core::Interfaces::LocateOption::End:
		if (offset <= 0 && offset >= -mFileSize)
			mCurrent = mMappedBase + (mFileSize - offset);
		break;
	}
}

RTME::MappedImportSession::~MappedImportSession()
{
	
}
