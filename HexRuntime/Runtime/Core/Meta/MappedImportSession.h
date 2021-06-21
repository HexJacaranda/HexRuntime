#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Interfaces\OSFile.h"
#include "IImportSession.h"
#include "MDHeap.h"

namespace RTM
{
	/// <summary>
	/// Normal import session using disk I/O
	/// </summary>
	class MappedImportSession :public IImportSession
	{
		UInt8* mMappedBase;
		UInt8* mCurrent;
		Int32 mFileSize;
	public:
		MappedImportSession(MDPrivateHeap* heap, RTI::FileMappingHandle handle, Int32 fileSize);
		virtual Int32 ReadInto(UInt8* memory, Int32 size);
		virtual void Relocate(Int32 offset, RTI::LocateOption option);
		virtual ~MappedImportSession();
	};
}