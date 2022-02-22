#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTM
{
	struct DispatchSlot
	{
		//[Interface Index][MethodDef Token]
		Int64 Key;
		Int32 Index;
		
		inline Int32 const& GetHash()const
		{
			return *((Int32*)this - 1);
		}
		inline Int32& GetHash()
		{
			return *((Int32*)this - 1);
		}
	};


	/// <summary>
	/// Kind of hashtable responsible for (Interface Index, Method Token) -> (MethodTable Index) mapping.
	///	When it's small, it behaves like a array on which we do linear search
	/// </summary>
	class InterfaceDispatchTable
	{
		//TODO: Use unordered_map
		friend class MetaManager;
		DispatchSlot* mSlots;
		//Highest bit is reserved for mode bit(linear or hash)
		Int32 mCount;
		void Put(Int32 interfaceIndex, MDToken token, Int32 index);
		Int32 GetHashCodeOf(Int64 key);
	public:
		bool IsHashMode()const;
		Int32 GetCount()const;
		Int32 GetIndex(Int32 interfaceIndex, MDToken token);	
	};
}