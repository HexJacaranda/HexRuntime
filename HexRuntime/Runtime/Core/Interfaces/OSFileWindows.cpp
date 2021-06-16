#include "OSFile.h"
#include <fileapi.h>
#include <handleapi.h>
#include <processthreadsapi.h>

RTI::FileHandle RTI::OSFile::Open(RTString filePath, Int8 usageOption, Int8 sharingOption)
{
	DWORD accessFlag = 0;
	if (usageOption & UsageOption::Read)
		accessFlag |= GENERIC_READ;
	if (usageOption & UsageOption::Write)
		accessFlag |= GENERIC_WRITE;

	DWORD shareFlag = 0;

	if (sharingOption & SharingOption::SharedRead)
		shareFlag |= FILE_SHARE_READ;
	if (sharingOption & SharingOption::SharedWrite)
		shareFlag |= FILE_SHARE_WRITE;

	return CreateFileW(filePath, accessFlag, shareFlag, nullptr, FILE_ATTRIBUTE_NORMAL, OPEN_EXISTING, nullptr);
}

void RTI::OSFile::Close(FileHandle& handle)
{
	CloseHandle(handle);
}

RTI::FileHandle RTI::OSFile::Duplicate(FileHandle& handle)
{
	auto currentProcess = GetCurrentProcess();
	FileHandle ret;
	DuplicateHandle(currentProcess, handle, currentProcess, &ret, 0, false, DUPLICATE_SAME_ACCESS);
	return ret;
}

void RTI::OSFile::Locate(FileHandle& handle, Int32 offset, LocateOption option)
{
	SetFilePointer(handle, offset, nullptr, (DWORD)option);
}

RT::Int32 RTI::OSFile::ReadInto(FileHandle& handle, UInt8* buffer, Int32 readBytes)
{
	DWORD doneBytes = 0;
	ReadFile(handle, buffer, readBytes, &doneBytes, nullptr);
	return doneBytes;
}
