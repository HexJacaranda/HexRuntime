#include "TypeDescriptor.h"
#include "CoreTypes.h"

RTO::StringObject* RTM::TypeDescriptor::GetTypeName() const
{
	return mTypeName;
}

RTO::StringObject* RTM::TypeDescriptor::GetNamespace() const
{
	return mNamespace;
}

RT::ObservableArray<RTM::TypeDescriptor*> RTM::TypeDescriptor::GetInterfaces() const
{
	return  { mInterfaces, mColdMD->InterfaceCount };
}

RT::ObservableArray<RTM::TypeDescriptor*> RTM::TypeDescriptor::GetTypeArguments() const
{
	return { mTypeArguments, 0 };
}

RT::UInt8 RTM::TypeDescriptor::GetCoreType() const
{
	return mCoreType;
}

RTM::TypeDescriptor* RTM::TypeDescriptor::GetParentType() const
{
	return mParent;
}

RTM::TypeDescriptor* RTM::TypeDescriptor::GetEnclosingType() const
{
	return mEnclosing;
}

RTM::TypeDescriptor* RTM::TypeDescriptor::GetCanonicalType() const
{
	return mCanonical;
}

RTM::FieldTable* RTM::TypeDescriptor::GetFieldTable() const
{
	return mFieldTable;
}

RTM::MethodTable* RTM::TypeDescriptor::GetMethodTable() const
{
	return mMethTable;
}

RTM::InterfaceDispatchTable* RTM::TypeDescriptor::GetInterfaceTable() const
{
	return mInterfaceTable;
}

RT::Int32 RTM::TypeDescriptor::GetSize() const
{
	return mFieldTable->GetLayout()->Size;
}

bool RTM::TypeDescriptor::IsArray() const
{
	return mCoreType == CoreTypes::Array;
}

bool RTM::TypeDescriptor::IsString() const
{
	return mCoreType == CoreTypes::String;
}
