#include "FieldDescriptor.h"

RTM::TypeDescriptor* RTM::FieldDescriptor::GetType() const
{
	return mFieldType;
}

RTO::StringObject* RTM::FieldDescriptor::GetName() const
{
	return mName;
}

bool RTM::FieldDescriptor::IsInstance() const
{
	return mColdMD->IsInstance();
}

bool RTM::FieldDescriptor::IsStatic() const
{
	return mColdMD->IsStatic();
}

bool RTM::FieldDescriptor::IsThreadLocal() const
{
	return mColdMD->IsThreadLocal();
}
