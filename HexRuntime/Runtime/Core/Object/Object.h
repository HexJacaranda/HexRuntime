#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTO
{
	class ObjectStorage;
}

namespace RTM
{
	class TypeDescriptor;
}

namespace RTO
{
	static constexpr Int32 BigObjectThreshold = 0xFFFF;
	static constexpr UInt32 BigObjectThresholdU = 0xFFFFu;
	static constexpr Int32 SmallestObjectSize = sizeof(void*) + sizeof(Int);
	class Object
	{
	private:
		RTM::TypeDescriptor* mType;
	public:
		RTM::TypeDescriptor* GetType()const;
		UInt32 GetObjectSize()const;
		ObjectStorage* GetStorage()const;
	};
	using ObjectRef = Object*;
}