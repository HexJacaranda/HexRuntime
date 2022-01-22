#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "Descriptor.h"
#include "MDRecords.h"

namespace RTM
{
	class TypeDescriptor;
	class FieldTable;
}

namespace RTM
{
	class FieldDescriptor : public Descriptor<RTME::FieldMD>
	{
		friend class MetaManager;
		RTO::StringObject* mName;
		TypeDescriptor* mFieldType;
		FieldTable* mOwningTable;
	public:
		TypeDescriptor* GetType()const;
		RTO::StringObject* GetName()const;
		FieldTable* GetOwningTable()const;

		bool IsStatic()const;
		bool IsThreadLocal()const;
		Int32 GetOffset()const;
	};
}