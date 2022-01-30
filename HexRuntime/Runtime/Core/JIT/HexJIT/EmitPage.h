#pragma once
#include "..\..\..\RuntimeAlias.h"

namespace RTJ
{
	class EmitPage
	{
		Int32 mLength = 0;
		UInt8* mRawPage = nullptr;
		Int32 mIndex = 0;
		Int32 mDataSegmentLength = 0;
	public:
		EmitPage();
		EmitPage(Int32 codeLength);
		EmitPage(Int32 alignedDataLength, Int32 codeLength);
		~EmitPage();
		Int32 CurrentOffset()const;
		Int32 GetCodeSize()const;
		UInt8* Prepare(Int32 length);
		void Commit(Int32 length);
		UInt8* GetRaw()const;
		UInt8* GetExecuteableCode()const;
		/// <summary>
		/// Set the code part of page to executable status and readonly for data
		/// </summary>
		/// <returns></returns>
		UInt8* Finalize();
	};
}