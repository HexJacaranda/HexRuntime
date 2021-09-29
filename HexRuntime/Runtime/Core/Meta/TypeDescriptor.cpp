#include "TypeDescriptor.h"
#include "CoreTypes.h"
#include "..\Exception\RuntimeException.h"

RTO::StringObject* RTM::TypeDescriptor::GetTypeName() const
{
	return mTypeName;
}

RTO::StringObject* RTM::TypeDescriptor::GetFullQualifiedName() const
{
	return mFullQualifiedName;
}

RT::ObservableArray<RTM::TypeDescriptor*> RTM::TypeDescriptor::GetInterfaces() const
{
	Int32 count = mColdMD->InterfaceCount;
	if (count == 1)
		return { &const_cast<TypeDescriptor*>(this)->mInterfaceInline, count };
	else
		return { mInterfaces, count };
}

RT::ObservableArray<RTM::TypeDescriptor*> RTM::TypeDescriptor::GetTypeArguments() const
{
	Int32 count = mColdMD->GenericParameterCount;
	if (count == 1)
		return { &const_cast<TypeDescriptor*>(this)->mTypeArgumentInline, count };
	else
		return { mTypeArguments, count };
}

RT::UInt8 RTM::TypeDescriptor::GetCoreType() const
{
	return mColdMD->CoreType;
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

RT::MDToken RTM::TypeDescriptor::GetToken() const
{
	return mSelf;
}

RTM::AssemblyContext* RTM::TypeDescriptor::GetAssembly() const
{
	return mContext;
}

RT::Int32 RTM::TypeDescriptor::GetSize() const
{
	return mFieldTable->GetLayout()->Size;
}

bool RTM::TypeDescriptor::IsArray() const
{
	return GetCoreType() == CoreTypes::Array;
}

bool RTM::TypeDescriptor::IsString() const
{
	return GetCoreType() == CoreTypes::String;
}

bool RTM::TypeDescriptor::IsInterface() const
{
	return mColdMD->IsInterface();
}

bool RTM::TypeDescriptor::IsSealed() const
{
	return mColdMD->IsSealed();
}

bool RTM::TypeDescriptor::IsStruct() const
{
	return mColdMD->IsStruct();
}

bool RTM::TypeDescriptor::IsAbstract() const
{
	return mColdMD->IsAbstract();
}

bool RTM::TypeDescriptor::IsAttribute() const
{
	return mColdMD->IsAttribute();
}

bool RTM::TypeDescriptor::IsGeneric() const
{
	return mColdMD->IsGeneric();
}

bool RTM::TypeDescriptor::IsAssignableFrom(TypeDescriptor* another) const
{
	if (another == nullptr)
		THROW("Invalid type descriptor");

	auto parentChain = another;
	while (parentChain != nullptr && this != parentChain)
		parentChain = parentChain->GetParentType();
	if (parentChain != nullptr)
		return true;

	//TODO: Loop unrolling to improve perf
	for (auto&& interface : another->GetInterfaces())
		if (interface == this)
			return true;

	return false;
}
