#include "InstantiationMap.h"

void RTM::InstantiationMap::AddScope(InstantiationLayerMap&& toGo)
{
	mLayeredMap.push_back(std::move(toGo));
}

bool RTM::InstantiationMap::TryGet(MDToken token, TypeDescriptor*& outValue)
{
	//Should be in order of top to bottom
	for (auto iterator = mLayeredMap.rbegin(); iterator != mLayeredMap.rend(); ++iterator)
	{
		auto where = iterator->find(token);
		if (where != iterator->end())
		{
			outValue = where->second;
			return true;
		}
	}

	return false;
}

void RTM::InstantiationMap::QuitScope()
{
	mLayeredMap.pop_back();
}