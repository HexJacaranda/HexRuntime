#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\ObservableArray.h"
#include "MDRecords.h"
#include "Descriptor.h"
#include "FieldTable.h"
#include "MethodTable.h"
#include "InterfaceDispatchTable.h"
#include <atomic>

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
	class TypeStatus
	{
	public:
		static constexpr Int8 NotYet = 0;
		static constexpr Int8 Processing = 1;
		static constexpr Int8 Basic = 2;
		//Intermediate status for better performance

		static constexpr Int8 LayoutDone = 3;
		static constexpr Int8 MethodTableDone = 4;
		static constexpr Int8 InterfaceTableDone = 5;
		/// <summary>
		/// Used for cyclical type loading
		/// </summary>
		static constexpr Int8 Almost = 6;
		static constexpr Int8 Done = 7;
	};

	class TypeDescriptor :public Descriptor<RTME::TypeMD>
	{
		friend class MetaManager;
		MDToken mSelf;

		RTO::StringObject* mTypeName = nullptr;
		RTO::StringObject* mFullQualifiedName = nullptr;
		TypeDescriptor* mParent = nullptr;
		TypeDescriptor* mEnclosing = nullptr;
		TypeDescriptor* mCanonical = nullptr;

		//Trick: inline storage
		union {
			TypeDescriptor** mInterfaces;
			TypeDescriptor* mInterfaceInline = nullptr;
		};
		
		union {
			TypeDescriptor** mTypeArguments;
			TypeDescriptor* mTypeArgumentInline = nullptr;
		};
		
		FieldTable* mFieldTable = nullptr;
		MethodTable* mMethTable = nullptr;
		InterfaceDispatchTable* mInterfaceTable = nullptr;
		
		AssemblyContext* mContext = nullptr;
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
		Int32 GetLayoutSize()const;

		bool IsArray()const;
		bool IsString()const;
		bool IsInterface()const;
		bool IsSealed()const;
		bool IsStruct()const;
		bool IsAbstract()const;
		bool IsAttribute()const;
		bool IsGeneric()const;

		bool IsAssignableFrom(TypeDescriptor* another)const;
	public:
		std::atomic<Int8> Status = TypeStatus::NotYet;
	};

	using Type = TypeDescriptor;
}
