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

		Int32 VirtualMethodCount;
		Int32 InstanceMethodCount;
		Int32 StaticMethodCount;
		MethodDescriptor* Methods;
	};
}