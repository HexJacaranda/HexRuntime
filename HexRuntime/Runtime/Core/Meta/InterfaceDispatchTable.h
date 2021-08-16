#pragma once
#include "..\..\RuntimeAlias.h"
#include <unordered_map>

namespace RTM
{
	/// <summary>
	/// Kind of hashtable responsible for (Interface Index, Method Token) -> (MethodTable Index) mapping.
	/// </summary>
	class InterfaceDispatchTable
	{
		friend class MetaManager;

		std::unordered_map<Int64, Int32> mMapping;
		void Put(Int32 interfaceIndex, MDToken token, Int32 index);
	public:
		Int32 GetCount()const;
		Int32 GetIndex(Int32 interfaceIndex, MDToken token);	
	};
}