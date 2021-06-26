#include "StringObject.h"

RTO::StringObject::StringObject(Int32 count):
    mCount(count)
{
}

RT::Int32 RTO::StringObject::GetCount() const
{
    return mCount;
}

RT::RTString RTO::StringObject::GetContent() const
{
    return (RTString)(this + 1);
}
