#include "InstantiationMap.h"

void RTM::InstantiationMap::AddScope(ScopeMap&& map)
{
	mTokenMap.push_back(std::move(map));
}

std::optional<RTM::Type*> RTM::InstantiationMap::TryGetFromReference(MDToken token) const
{
	TypedToken key{ RTME::MDRecordKinds::TypeRef, token };
	//Should be in order of top to bottom
	for (auto iterator = mTokenMap.rbegin(); iterator != mTokenMap.rend(); ++iterator)
		if (auto where = iterator->find(key); where != iterator->end())
			return where->second;

	return {};
}

std::optional<RTM::Type*> RTM::InstantiationMap::TryGetFromDefinition(MDToken token) const
{
	TypedToken key{ RTME::MDRecordKinds::GenericParameter, token };
	//Should be in order of top to bottom
	for (auto iterator = mTokenMap.rbegin(); iterator != mTokenMap.rend(); ++iterator)
		if (auto where = iterator->find(key); where != iterator->end())
			return where->second;

	return {};
}

RT::Int32 RTM::InstantiationMap::GetCurrentScopeArgCount() const
{
	if (mTokenMap.size() == 0) 
		return 0;
	return mTokenMap.back().size();
}

void RTM::InstantiationMap::QuitScope()
{
	mTokenMap.pop_back();
}