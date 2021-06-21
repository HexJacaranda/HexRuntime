#include "OSFile.h"
#include <fileapi.h>
#include <handleapi.h>
#include <processthreadsapi.h>
#include <memoryapi.h>

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

void RTI::OSFile::Close(FileHandle handle)
{
	CloseHandle(handle);
}

RTI::FileMappingHandle RTI::OSFile::OpenMapping(FileHandle handle, Int8 usageOption)
{
	DWORD accessFlag = 0;
	if (usageOption == (UsageOption::Read | UsageOption::Write))
		accessFlag = PAGE_READWRITE;
	else if (usageOption & UsageOption::Read)
		accessFlag |= PAGE_READONLY;
	else if (usageOption & UsageOption::Write)
		accessFlag |= PAGE_WRITECOPY;

	return CreateFileMapping(handle, nullptr, accessFlag, 0, 0, nullptr);
}

void RTI::OSFile::CloseMapping(FileMappingHandle handle)
{
	CloseHandle(handle);
}

RT::UInt8* RTI::OSFile::MapAddress(FileMappingHandle handle, Int8 usageOption)
{
	DWORD accessFlag = 0;
	if (usageOption & UsageOption::Read)
		accessFlag |= FILE_MAP_READ;
	if (usageOption & UsageOption::Write)
		accessFlag |= FILE_MAP_WRITE;
	//Map all
	return (UInt8*)MapViewOfFile(handle, accessFlag, 0, 0, 0);
}

void RTI::OSFile::UnmapAddress(UInt8* address)
{
	UnmapViewOfFile(address);
}

RTI::FileHandle RTI::OSFile::Duplicate(FileHandle handle)
{
	auto currentProcess = GetCurrentProcess();
	FileHandle ret;
	DuplicateHandle(currentProcess, handle, currentProcess, &ret, 0, false, DUPLICATE_SAME_ACCESS);
	return ret;
}

void RTI::OSFile::Locate(FileHandle handle, Int32 offset, LocateOption option)
{
	SetFilePointer(handle, offset, nullptr, (DWORD)option);
}

RT::Int32 RTI::OSFile::ReadInto(FileHandle handle, UInt8* buffer, Int32 readBytes)
{
	DWORD doneBytes = 0;
	ReadFile(handle, buffer, readBytes, &doneBytes, nullptr);
	return doneBytes;
}

RT::Int32 RTI::OSFile::SizeOf(FileHandle handle)
{
	return GetFileSize(handle, nullptr);
}
