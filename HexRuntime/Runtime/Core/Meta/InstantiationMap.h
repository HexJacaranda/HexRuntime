#pragma once
#include "..\..\RuntimeAlias.h"
#include "TypeDescriptor.h"
#include <unordered_map>
#include <deque>

namespace RTM
{
	using InstantiationLayerMap = std::unordered_map<MDToken, TypeDescriptor*>;
	class InstantiationMap
	{
		std::deque<InstantiationLayerMap> mLayeredMap;
	public:
		void AddScope(InstantiationLayerMap&& toGo);
		bool TryGet(MDToken token, TypeDescriptor*& outValue);
		void QuitScope();
	};

	class InstantiationSession
	{
		InstantiationMap& mMap;
	public:
		InstantiationSession(InstantiationMap& map, InstantiationLayerMap&& toGo)
			:mMap(map)
		{
			mMap.AddScope(std::move(toGo));
		}
		~InstantiationSession()
		{
			mMap.QuitScope();
		}
	};

#define INSTANTIATION_SESSION(LAYER) RTM::InstantiationSession _session { genericMap, std::move(LAYER) }
}