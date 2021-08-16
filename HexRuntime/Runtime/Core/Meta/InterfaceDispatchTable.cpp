#include "InterfaceDispatchTable.h"

RT::Int32 RTM::InterfaceDispatchTable::GetCount() const
{
	return mMapping.size();
}

RT::Int32 RTM::InterfaceDispatchTable::GetIndex(Int32 interfaceIndex, MDToken token)
{
	Int64 key = (((Int64)interfaceIndex) << 32) | (Int64)token;
	return mMapping.find(key)->second;
}

void RTM::InterfaceDispatchTable::Put(Int32 interfaceIndex, MDToken token, Int32 methodIndex)
{
	Int64 key = (((Int64)interfaceIndex) << 32) | (Int64)token;
	mMapping.insert_or_assign(key, methodIndex);
}
