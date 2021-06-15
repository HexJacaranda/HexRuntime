#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTI
{
	enum class UsageOption
	{
		Read = 0x01,
		Write = 0x02
	};

	enum class LocateOption
	{
		Start,
		End,
		Current
	};

	using FileHandle = void*;

	class OSFile
	{
	public:
		static FileHandle Open(RTString filePath, UsageOption usageOption);
		static void Close(FileHandle& handle);
		static void Locate(FileHandle& handle, Int64 offset, LocateOption option);
		static void ReadInto(FileHandle& handle, UInt8* buffer, Int32 readBytes);
	};
}