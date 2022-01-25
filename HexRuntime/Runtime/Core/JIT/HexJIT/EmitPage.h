#pragma once
#include "..\..\..\RuntimeAlias.h"

namespace RTJ
{
	class EmitPage
	{
		Int32 mLength = 0;
		UInt8* mRawPage = nullptr;
		Int32 mIndex = 0;
	public:
		EmitPage();
		EmitPage(Int32 length);
		~EmitPage();
		Int32 CurrentOffset()const;
		UInt8* Prepare(Int32 length);
		void Commit(Int32 length);
		UInt8* GetRaw()const;
		/// <summary>
		/// Set the page to executable status
		/// </summary>
		/// <returns></returns>
		UInt8* Finalize();
	};
}