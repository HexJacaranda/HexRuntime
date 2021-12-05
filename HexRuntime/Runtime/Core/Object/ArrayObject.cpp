#include "ArrayObject.h"
#include "..\Meta\TypeDescriptor.h"

RT::UInt32 RTO::ArrayObject::GetCount() const
{
    return mCount;
}

RT::Int8* RTO::ArrayObject::GetElementAddress() const
{
    return (Int8*)(this + 1);
}

RTM::Type* RTO::ArrayObject::GetElementType() const
{
    return GetType()->GetTypeArguments()[0];
}

bool RTO::ArrayObject::IsMultiDimensionalArray() const
{
    return mDimension > 1;
}

bool RTO::ArrayObject::IsSZArray() const
{
    return mDimension == 1 && mLowerBounds == 0;
}
