#include "OSFile.h"

RTI::FileHandle RTI::OSFile::Open(RTString filePath, UsageOption usageOption)
{
	return FileHandle();
}

void RTI::OSFile::Close(FileHandle& handle)
{
}

void RTI::OSFile::Locate(FileHandle& handle, Int64 offset, LocateOption option)
{
}

void RTI::OSFile::ReadInto(FileHandle& handle, UInt8* buffer, Int32 readBytes)
{
}
