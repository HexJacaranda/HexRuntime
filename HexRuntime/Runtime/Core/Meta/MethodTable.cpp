#include "MethodTable.h"
#include "..\Exception\RuntimeException.h"

RT::ObservableArray<RTM::MethodDescriptor*> RTM::MethodTable::GetVirtualMethods() const
{
    return { mMethods, mVirtualMethodCount };
}

RT::ObservableArray<RTM::MethodDescriptor*> RTM::MethodTable::GetNonVirtualMethods() const
{
    return { mMethods + mVirtualMethodCount, mInstanceMethodCount };
}

RT::ObservableArray<RTM::MethodDescriptor*> RTM::MethodTable::GetStaticMethods() const
{
    return { mMethods + mVirtualMethodCount + mInstanceMethodCount, mStaticMethodCount };
}

RT::ObservableArray<RTM::MethodDescriptor*> RTM::MethodTable::GetInstanceMethods() const
{
    return { mMethods, mVirtualMethodCount + mInstanceMethodCount };
}

RT::ObservableArray<RTM::MethodDescriptor*> RTM::MethodTable::GetMethods() const
{
    return { mMethods, GetCount() };
}

RT::ObservableArray<RTM::MethodDescriptor*> RTM::MethodTable::GetOverridenRegion() const
{
    return { mOverridenRegion, mOverridenRegionCount };
}

RT::Int32 RTM::MethodTable::GetCount() const
{
    return  mVirtualMethodCount + mInstanceMethodCount + mStaticMethodCount;
}

RTM::MethodDescriptor* RTM::MethodTable::GetMethodBy(MDToken methodDefToken)
{
    if (methodDefToken == NullToken ||
        methodDefToken < mBaseMethodToken ||
        (methodDefToken - mBaseMethodToken > GetCount()))
        THROW("Token out of ranges");

    return mMethods[methodDefToken - mBaseMethodToken];
}

RT::Int32 RTM::MethodTable::GetMethodIndexBy(MDToken methodDefToken)
{
    if (methodDefToken == NullToken ||
        methodDefToken < mBaseMethodToken ||
        (methodDefToken - mBaseMethodToken > GetCount()))
        THROW("Token out of ranges");

    return methodDefToken - mBaseMethodToken;
}
