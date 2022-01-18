#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "..\..\ObservableArray.h"
#include "..\Platform\PlatformSpecialization.h"
#include "MDRecords.h"
#include "Descriptor.h"

namespace RTM
{
	class TypeDescriptor;
	class MethodTable;
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
		RTO::StringObject* mManagedName = nullptr;
		MethodSignatureDescriptor* mSignature = nullptr;
		MethodLocalVariableDescriptor* mLocals = nullptr;
		Int32 mOverrideRedirectIndex = -1;
		MDToken mSelf = NullToken;
		MethodTable* mOwningTable = nullptr;
		RTP::PlatformCallingConvention* mCallingConv = nullptr;
	public:
		UInt8 GetAccessbility()const;
		MethodSignatureDescriptor* GetSignature()const;
		ObservableArray<MethodLocalVariableDescriptor> GetLocalVariables()const;
		RTME::ILMD* GetIL()const;
		TypeDescriptor* GetReturnType()const;
		ObservableArray<MethodArgumentDescriptor> GetArguments()const;
		MDToken GetDefToken()const;
		MethodTable* GetOwningTable()const;
		RTO::StringObject* GetName()const;
		bool IsStatic()const;
		bool IsVirtual()const;
		bool IsOverride()const;
		bool IsFinal()const;
		bool IsGeneric()const;

		/// <summary>
		/// Currently will only be used by JIT, so it's safe to use relaxed memory order
		/// </summary>
		/// <param name="callingConv"></param>
		void SetCallingConvention(RTP::PlatformCallingConvention* callingConv);
		RTP::PlatformCallingConvention* GetCallingConvention()const;
	};
}