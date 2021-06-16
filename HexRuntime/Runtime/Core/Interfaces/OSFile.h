#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTI
{
	class UsageOption
	{
	public:
		static constexpr Int8 Read = 0x01;
		static constexpr Int8 Write = 0x02;
	};

	class SharingOption
	{
	public:
		static constexpr Int8 Exculsive = 0x00;
		static constexpr Int8 SharedRead = 0x01;
		static constexpr Int8 SharedWrite = 0x02;
	};

	

	enum class LocateOption
	{
		Start,
		Current,
		End
	};

	using FileHandle = void*;

	class OSFile
	{
	public:
		static FileHandle Open(RTString filePath, Int8 usageOption, Int8 sharingOption);
		static void Close(FileHandle& handle);
		static FileHandle Duplicate(FileHandle& handle);
		static void Locate(FileHandle& handle, Int32 offset, LocateOption option);
		static Int32 ReadInto(FileHandle& handle, UInt8* buffer, Int32 readBytes);
	};
}