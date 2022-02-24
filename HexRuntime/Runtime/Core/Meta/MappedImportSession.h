#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Interfaces\OSFile.h"
#include "IImportSession.h"
#include "..\Memory\PrivateHeap.h"

namespace RTME
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
		MappedImportSession(RTMM::PrivateHeap* heap, UInt8* address, Int32 fileSize);
		virtual Int32 ReadInto(UInt8* memory, Int32 size);
		virtual void Relocate(Int32 offset, RTI::LocateOption option);
		virtual Int32 GetCurrentPosition()const;
		virtual ~MappedImportSession();
	};
}