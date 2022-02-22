#include "InstantiationMap.h"

void RTM::InstantiationMap::AddRefScope(InstantiationLayerMap&& refMap)
{
	mRefTokenMap.push_back(std::move(refMap));
}

void RTM::InstantiationMap::AddDefScope(InstantiationLayerMap&& defMap)
{
	mDefTokenMap.push_back(std::move(defMap));
}

std::optional<RTM::Type*> RTM::InstantiationMap::TryGetFromReference(MDToken token)
{
	//Should be in order of top to bottom
	for (auto iterator = mRefTokenMap.rbegin(); iterator != mRefTokenMap.rend(); ++iterator)
		if (auto where = iterator->find(token); where != iterator->end())
			return where->second;

	return {};
}

std::optional<RTM::Type*> RTM::InstantiationMap::TryGetFromDefinition(MDToken token)
{
	//Should be in order of top to bottom
	for (auto iterator = mDefTokenMap.rbegin(); iterator != mDefTokenMap.rend(); ++iterator)
		if (auto where = iterator->find(token); where != iterator->end())
			return where->second;

	return {};
}

void RTM::InstantiationMap::QuitDefScope()
{
	mDefTokenMap.pop_back();
}

void RTM::InstantiationMap::QuitRefScope()
{
	mRefTokenMap.pop_back();
}