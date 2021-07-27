#include "InterfaceDispatchTable.h"

bool RTM::InterfaceDispatchTable::IsHashMode() const
{
	return !!(mCount & 0x80000000);
}

RT::Int32 RTM::InterfaceDispatchTable::GetCount() const
{
	return mCount & 0x7FFFFFFF;
}

RT::Int32 RTM::InterfaceDispatchTable::GetIndex(Int32 interfaceIndex, MDToken token)
{
	Int64 key = (((Int64)interfaceIndex) << 32) | (Int64)token;
	Int32 count = GetCount();
	if (IsHashMode())
	{
		Int32 index, startIndex;
		Int32 hash = GetHashCodeOf(key);
		index = hash;
		startIndex = hash % count;
		do 
		{
			index = index % count;
			auto&& slot = mSlots[index];
			if (slot.GetHash() == hash && slot.Key == key)
				return slot.Index;
			index++;
		} while (index != startIndex);
	}
	else
	{
		for (Int32 i = 0; i < count; ++i)
			if (mSlots[i].Key == i)
				return mSlots[i].Index;
	}
	return -1;
}

void RTM::InterfaceDispatchTable::Put(Int32 interfaceIndex, MDToken token, Int32 index)
{
	Int64 key = (((Int64)interfaceIndex) << 32) | (Int64)token;
	Int32 index, startIndex;
	Int32 hash = GetHashCodeOf(key);
	Int32 count = GetCount();
	index = hash;
	startIndex = hash % count;
	do
	{
		index = index % count;
		auto&& slot = mSlots[index];
		if (slot.GetHash() == 0)
		{
			slot.Key = key;
			slot.Index = index;
			break;
		}
		index++;
	} while (index != startIndex);
}

RT::Int32 RTM::InterfaceDispatchTable::GetHashCodeOf(Int64 key)
{
#define XOR_SHIFT(n, i)  (n) ^ ((n) >> i) 
	UInt64 p = 0x5555555555555555ull; 
	UInt64 c = 17316035218449499591ull;
	Int32 hash = 0;
	while (hash == 0)
	{
		UInt64 full = c * XOR_SHIFT(p * XOR_SHIFT(key, 32), 32);
		hash = (Int32)(full & 0x7FFFFFFF);
	}	
#undef XOR_SHIFT
	return hash;
}
