#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "..\..\ObservableArray.h"
#include "MDRecords.h"
#include "Descriptor.h"

namespace RTM
{
	class TypeDescriptor;
}

namespace RTM
{
	class MethodArgumentDescriptor : public Descriptor<RTME::ArgumentMD>
	{
		friend class MetaManager;
		TypeDescriptor* mType;
		RTO::StringObject* mManagedName;
	public:
		TypeDescriptor* GetType();
		RTO::StringObject* GetName();
	};

	class MethodLocalVariableDescriptor : public Descriptor<RTME::LocalVariableMD>
	{
		friend class MetaManager;
		TypeDescriptor* mType;
		RTO::StringObject* mManagedName;
	public:
		TypeDescriptor* GetType();
		RTO::StringObject* GetName();
	};

	class MethodSignatureDescriptor : public Descriptor<RTME::MethodSignatureMD>
	{
		friend class MetaManager;
		TypeDescriptor* mReturnType;
		MethodArgumentDescriptor* mArguments;
	public:
		ObservableArray<MethodArgumentDescriptor> GetArguments()const;
		TypeDescriptor* GetReturnType()const;
	};

	/// <summary>
	/// Runtime structure for a real method
	/// </summary>
	class MethodDescriptor : public Descriptor<RTME::MethodMD>
	{
		friend class MetaManager;
		RTO::StringObject* mManagedName;
		MethodSignatureDescriptor* mSignature;
		MethodLocalVariableDescriptor* mLocals;
		Int32 mOverrideRedirectIndex = -1;
		MDToken mSelf;
	public:
		UInt8 GetAccessbility()const;
		MethodSignatureDescriptor* GetSignature()const;
		ObservableArray<MethodLocalVariableDescriptor> GetLocalVariables()const;
		RTME::ILMD* GetIL()const;
		MDToken GetDefToken()const;
		RTO::StringObject* GetName()const;
		bool IsStatic()const;
		bool IsVirtual()const;
		bool IsOverride()const;
		bool IsFinal()const;
		bool IsGeneric()const;
	};
}