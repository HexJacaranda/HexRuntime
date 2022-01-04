#pragma once
#include "..\..\..\RuntimeAlias.h"

namespace RTJ
{
	class EmitPage
	{
		Int32 mLength;
		UInt8* mRawPage;
		Int32 mIndex;
	public:
		Int32 CurrentOffset()const;
		UInt8* Prepare(Int32 length);
		void Commit(Int32 length);
		/// <summary>
		/// Set the page to executable status
		/// </summary>
		/// <returns></returns>
		UInt8* Finalize();
	};
}