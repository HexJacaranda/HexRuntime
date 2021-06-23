#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Object\StringObject.h"
#include "MDRecords.h"

namespace RTM
{
	/// <summary>
	/// Runtime structure for a real method
	/// </summary>
	class MethodDescriptor
	{
		friend class MetaManager;
		MethodMD* mColdMD;
		RTO::StringObject* mManagedName;
		RTM::ArgumentMD* mArguments;
	public:
		MethodMD* GetMetadata()const;
		UInt8 GetAccessbility()const;
		MethodSignatureMD* GetSignature()const;
		ILMD* GetIL()const;
		bool IsInstance()const;
		bool IsVirtual()const;
		bool IsOverride()const;
		bool IsFinal()const;
		bool IsGeneric()const;
	};
}