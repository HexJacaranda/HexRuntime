#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "Descriptor.h"
#include "MDRecords.h"

namespace RTM
{
	class TypeDescriptor;
}

namespace RTM
{
	class FieldDescriptor : public Descriptor<RTME::FieldMD>
	{
		friend class MetaManager;
		RTO::StringObject* mName;
		TypeDescriptor* mFieldType;
	public:
		TypeDescriptor* GetType()const;
		RTO::StringObject* GetName()const;

		bool IsStatic()const;
		bool IsThreadLocal()const;
	};
}