#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\ObservableArray.h"
#include "MDRecords.h"
#include "Descriptor.h"
#include "FieldTable.h"
#include "MethodTable.h"
#include "InterfaceDispatchTable.h"

namespace RTO
{
	class StringObject;
}

namespace RTM
{
	struct AssemblyContext;
}

namespace RTM
{
	class TypeDescriptor :public Descriptor<RTME::TypeMD>
	{
		friend class MetaManager;
		MDToken mSelf;

		RTO::StringObject* mTypeName;
		RTO::StringObject* mFullQualifiedName;
		TypeDescriptor* mParent;
		TypeDescriptor* mEnclosing;
		TypeDescriptor* mCanonical;
		TypeDescriptor** mInterfaces;

		TypeDescriptor** mTypeArguments;

		FieldTable* mFieldTable;
		MethodTable* mMethTable;
		InterfaceDispatchTable* mInterfaceTable;
		
		AssemblyContext* mContext;
	public:
		RTO::StringObject* GetTypeName()const;
		RTO::StringObject* GetFullQualifiedName()const;
		ObservableArray<TypeDescriptor*> GetInterfaces()const;
		ObservableArray<TypeDescriptor*> GetTypeArguments()const;
		UInt8 GetCoreType()const;
		TypeDescriptor* GetParentType()const;
		TypeDescriptor* GetEnclosingType()const;
		TypeDescriptor* GetCanonicalType()const;
		FieldTable* GetFieldTable()const;
		MethodTable* GetMethodTable()const;
		InterfaceDispatchTable* GetInterfaceTable()const;
		MDToken GetToken()const;
		AssemblyContext* GetAssembly()const;

		Int32 GetSize()const;

		bool IsArray()const;
		bool IsString()const;
		bool IsInterface()const;
		bool IsSealed()const;
		bool IsStruct()const;
		bool IsAbstract()const;
		bool IsAttribute()const;
		bool IsGeneric()const;

		bool IsAssignableFrom(TypeDescriptor* another)const;
	};

	using Type = TypeDescriptor;
}
