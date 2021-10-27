#include "StringObject.h"

RTO::StringObject::StringObject(Int32 count):
    mCount(count)
{
}

RTO::StringObject::StringObject(Int32 count, RTString value):
    mCount(count)
{
    auto mutableContent = (MutableRTString)GetContent();
    std::memcpy(mutableContent, value, sizeof(wchar_t) * count);
    mutableContent[count] = L'\0';
}

RT::Int32 RTO::StringObject::GetCount() const
{
    return mCount;
}

RT::RTString RTO::StringObject::GetContent() const
{
    return (RTString)(this + 1);
}
