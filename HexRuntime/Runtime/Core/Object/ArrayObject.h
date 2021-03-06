#pragma once
#include "..\..\RuntimeAlias.h"
#include "Object.h"
#include "..\Meta\TypeDescriptor.h"

namespace RTO
{
	class ArrayObject : public Object
	{
	private:
		UInt32 mCount;
		UInt8 mDimension;
		UInt8 mLowerBounds;
		UInt16 mFlags;
		union
		{
			UInt32* mUpperBounds;
			UInt32 mSingleUpperBound;
		};	
	public:
		UInt32 GetCount()const;
		Int8* GetElementAddress()const;
		RTM::Type* GetElementType()const;
		bool IsMultiDimensionalArray()const;
		bool IsSZArray()const;
	};
}