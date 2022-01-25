#include "EmitPage.h"
#include "EmitPageProvider.h"
#include "..\..\Exception\RuntimeException.h"

namespace RTJ
{
	EmitPage::EmitPage() :EmitPage(16)
	{
	}

	EmitPage::EmitPage(Int32 length): mLength(length)
	{
		mRawPage = EmitPageProvider::Allocate(length);
	}

	EmitPage::~EmitPage()
	{
		if (mRawPage != nullptr)
		{
			mLength = 0;
			mIndex = 0;
			EmitPageProvider::Free(mRawPage);
		}
	}

	Int32 EmitPage::CurrentOffset() const
	{
		return mIndex;
	}

	UInt8* EmitPage::Prepare(Int32 length)
	{
		if (mIndex + length >= mLength) {
			UInt8* newSpace = EmitPageProvider::Allocate(mLength << 1);
			if (newSpace == nullptr)
				THROW("Unable to allocate virtual pages for native code generation");
			memcpy_s(newSpace, ((UInt64)mLength) << 1, mRawPage, mIndex);
			EmitPageProvider::Free(mRawPage);
			mRawPage = newSpace;
			mLength <<= 1;
		}
		return mRawPage + mIndex;
	}

	void EmitPage::Commit(Int32 length)
	{
		mIndex += length;
	}

	UInt8* EmitPage::GetRaw() const
	{
		return mRawPage;
	}

	UInt8* EmitPage::Finalize()
	{
		return EmitPageProvider::SetExecutable(mRawPage, mIndex);
	}
}