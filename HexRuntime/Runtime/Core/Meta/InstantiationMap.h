#pragma once
#include "..\..\RuntimeAlias.h"
#include "TypeDescriptor.h"
#include <unordered_map>
#include <deque>

namespace RTM
{
	struct TypedToken
	{
		RTME::MDRecordKinds Kind;
		MDToken Token;
	};

	struct TypedTokenHash
	{
		std::size_t operator()(TypedToken const& token)const {
			return ComputeHashCode(token.Kind) + ComputeHashCode(token.Token);
		}
	};

	struct TypedTokenEqual
	{
		bool operator()(TypedToken const& left, TypedToken const& right)const {
			return left.Kind == right.Kind && left.Token == right.Token;
		}
	};

	using ScopeMap = std::unordered_map<TypedToken, TypeDescriptor*, TypedTokenHash, TypedTokenEqual>;

	class InstantiationMap
	{
		std::deque<ScopeMap> mTokenMap;
	public:
		void AddScope(ScopeMap&& defMap);
		std::optional<TypeDescriptor*> TryGetFromReference(MDToken token) const;
		std::optional<TypeDescriptor*> TryGetFromDefinition(MDToken token) const;
		Int32 GetCurrentScopeArgCount()const;
		void QuitScope();
	};

	class InstantiationSession
	{
		InstantiationMap& mMap;
	public:
		InstantiationSession(
			InstantiationMap& map, 
			ScopeMap&& tokenMap)
			:mMap(map)
		{
			mMap.AddScope(std::move(tokenMap));
		}
		~InstantiationSession()
		{
			mMap.QuitScope();
		}
	};

#define INSTANTIATION_SCOPE(LAYER) RTM::InstantiationSession _session { genericMap, std::move(LAYER) }
}