#pragma once
#include "..\..\RuntimeAlias.h"
#include "IImportSession.h"
#include "..\Memory\PrivateHeap.h"
#include "..\Interfaces\OSFile.h"

namespace RTME
{
	class ImportSession :public IImportSession
	{
		RTI::FileHandle mHandle;
	public:
		ImportSession(RTMM::PrivateHeap* heap, RTI::FileHandle handle);
		virtual Int32 ReadInto(UInt8* memory, Int32 size);
		virtual void Relocate(Int32 offset, RTI::LocateOption option);
		virtual Int32 GetCurrentPosition()const;
		virtual ~ImportSession();
	};
}