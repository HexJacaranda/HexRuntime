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
		std::deque<InstantiationLayerMap> mRefTokenMap;
		std::deque<InstantiationLayerMap> mDefTokenMap;
	public:
		void AddRefScope(InstantiationLayerMap&& refMap);
		void AddDefScope(InstantiationLayerMap&& defMap);
		std::optional<TypeDescriptor*> TryGetFromReference(MDToken token);
		std::optional<TypeDescriptor*> TryGetFromDefinition(MDToken token);
		void QuitRefScope();
		void QuitDefScope();
	};

	class InstantiationSession
	{
		InstantiationMap& mMap;
		bool mIsDef;
	public:
		InstantiationSession(
			InstantiationMap& map, 
			InstantiationLayerMap&& tokenMap,
			bool isDef = false)
			:mMap(map), mIsDef(isDef)
		{
			if (isDef)
				mMap.AddDefScope(std::move(tokenMap));
			else
				mMap.AddRefScope(std::move(tokenMap));		
		}
		~InstantiationSession()
		{
			if (mIsDef)
				mMap.QuitDefScope();
			else
				mMap.QuitRefScope();
		}
	};

#define INSTANTIATION_SESSION(LAYER) RTM::InstantiationSession _session { genericMap, std::move(LAYER) }
#define INSTANTIATION_SESSION_DEF(LAYER) RTM::InstantiationSession _session { genericMap, std::move(LAYER), true }
}