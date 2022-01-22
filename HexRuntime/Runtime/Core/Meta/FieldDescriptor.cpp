#include "FieldDescriptor.h"
#include "FieldTable.h"

RTM::TypeDescriptor* RTM::FieldDescriptor::GetType() const
{
	return mFieldType;
}

RTO::StringObject* RTM::FieldDescriptor::GetName() const
{
	return mName;
}

bool RTM::FieldDescriptor::IsStatic() const
{
	return mColdMD->IsStatic();
}

bool RTM::FieldDescriptor::IsThreadLocal() const
{
	return mColdMD->IsThreadLocal();
}

RT::Int32 RTM::FieldDescriptor::GetOffset() const
{
	Int32 index = this - mOwningTable->mFields;
	return mOwningTable->GetLayout()->Slots[index].Offset;
}

RTM::FieldTable* RTM::FieldDescriptor::GetOwningTable() const
{
	return mOwningTable;
}
