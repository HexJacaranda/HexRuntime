#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "..\..\ObservableArray.h"
#include "MDRecords.h"
#include "Descriptor.h"

namespace RTM
{
	struct MethodArgument : public Descriptor<RTME::ArgumentMD>
	{

	};

	struct MethodSignature : public Descriptor<RTME::MethodSignatureMD>
	{

	};

	/// <summary>
	/// Runtime structure for a real method
	/// </summary>
	class MethodDescriptor : public Descriptor<RTME::MethodMD>
	{
		friend class MetaManager;
		RTO::StringObject* mManagedName;
		MethodSignature* mSignature;
		RTME::ArgumentMD* mArguments;
	public:
		UInt8 GetAccessbility()const;
		MethodSignature* GetSignature()const;
		ObservableArray<RTME::ArgumentMD> GetArguments()const;
		ObservableArray<RTME::LocalVariableMD> GetLocalVariables()const;
		RTME::ILMD* GetIL()const;
		bool IsInstance()const;
		bool IsVirtual()const;
		bool IsOverride()const;
		bool IsFinal()const;
		bool IsGeneric()const;
	};
}