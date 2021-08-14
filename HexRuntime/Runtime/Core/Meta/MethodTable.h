#pragma once
#include "..\..\RuntimeAlias.h"
#include "MethodDescriptor.h"

namespace RTM
{
	/// <summary>
	/// Partitioned by three parts
	/// virtual, instance, static
	/// </summary>
	class MethodTable
	{
		friend class MetaManager;

		Int32 mVirtualMethodCount;
		Int32 mInstanceMethodCount;
		Int32 mStaticMethodCount;
		Int32 mOverridenRegionCount;
		MethodDescriptor** mOverridenRegion;
		MethodDescriptor** mMethods;
		MDToken mBaseMethodToken;
	public:
		ObservableArray<MethodDescriptor*> GetVirtualMethods()const;
		ObservableArray<MethodDescriptor*> GetNonVirtualMethods()const;
		ObservableArray<MethodDescriptor*> GetStaticMethods()const;
		ObservableArray<MethodDescriptor*> GetInstanceMethods()const;
		ObservableArray<MethodDescriptor*> GetMethods()const;
		ObservableArray<MethodDescriptor*> GetOverridenRegion()const;
		Int32 GetCount()const;
		MethodDescriptor* GetMethodBy(MDToken methodDefToken);
		Int32 GetMethodIndexBy(MDToken methodDefToken);
	};
}