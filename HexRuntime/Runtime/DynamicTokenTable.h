#pragma once
#include "RuntimeAlias.h"
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <deque>

namespace RT
{
	template<class IdentityT, class MetaT, class IdentityHasher = std::hash<IdentityT>, class IdentityEqualer = std::equal_to<IdentityT>>
	class DynamicMetaTable
	{
		MetaT* mCompiledMetaTokenTable = nullptr;
		MDToken mCompiledMetaTokenCount = 0;
		std::pmr::deque<MetaT*> mDynamicMetaTokenTable;
		std::pmr::unordered_map<IdentityT, MDToken> mIdentityMap;
		mutable std::shared_mutex mDynamicLock;
	private:
		MetaT& GetMetaByTokenUnsafe(MDToken token) {
			if (token <= mCompiledMetaTokenCount)
				return mCompiledMetaTokenTable[token];

			MDToken dynamicIndex = token - mCompiledMetaTokenCount;
			return *mDynamicMetaTokenTable[dynamicIndex];
		}
		MetaT const& GetMetaByTokenUnsafe(MDToken token)const {
			if (token <= mCompiledMetaTokenCount)
				return mCompiledMetaTokenTable[token];

			MDToken dynamicIndex = token - mCompiledMetaTokenCount;
			return *mDynamicMetaTokenTable[dynamicIndex];
		}
	public:
		DynamicMetaTable(DynamicMetaTable const&) = delete;
		DynamicMetaTable(DynamicMetaTable&&) = default;
		DynamicMetaTable& operator=(DynamicMetaTable const&) = delete;
		DynamicMetaTable& operator=(DynamicMetaTable&& another)
		{
			mCompiledMetaTokenTable = another.mCompiledMetaTokenTable;
			mCompiledMetaTokenCount = another.mCompiledMetaTokenCount;
			mDynamicMetaTokenTable = std::move(another.mDynamicMetaTokenTable);
			mIdentityMap = std::move(another.mIdentityMap);
			return *this;
		}

		DynamicMetaTable(std::pmr::synchronized_pool_resource* resource) :
			mDynamicMetaTokenTable(resource),
			mIdentityMap(resource) {}

		DynamicMetaTable(
			MetaT* meta,
			MDToken count,
			std::pmr::synchronized_pool_resource* resource) :
			mCompiledMetaTokenTable(meta),
			mCompiledMetaTokenCount(count),
			mDynamicMetaTokenTable(resource),
			mIdentityMap(resource) {}

		MetaT& operator[](MDToken token) {
			if (token <= mCompiledMetaTokenCount)
				return mCompiledMetaTokenTable[token];
			
			MDToken dynamicIndex = token - mCompiledMetaTokenCount;
			std::shared_lock lock{ mDynamicLock };
			return *mDynamicMetaTokenTable[dynamicIndex];
		}

		MetaT const& operator[](MDToken token)const {
			if (token <= mCompiledMetaTokenCount)
				return mCompiledMetaTokenTable[token];

			MDToken dynamicIndex = token - mCompiledMetaTokenCount;
			std::shared_lock lock{ mDynamicLock };
			return *mDynamicMetaTokenTable[dynamicIndex];
		}

		MDToken operator[](IdentityT const& identity)const {
			return mIdentityMap.at(identity);
		}

		bool DefineMetaMapUnsafe(IdentityT const& identity, MDToken token)
		{
			if (auto where = mIdentityMap.find(identity); where != mIdentityMap.end())
				return false;
			mIdentityMap[identity] = token;
			return true;
		}

		std::tuple<MDToken, MetaT*> DefineNewMeta(IdentityT const& identity, MetaT* newMeta)
		{
			{
				std::shared_lock lock{ mDynamicLock };

				if (auto where = mIdentityMap.find(identity);
					where != mIdentityMap.end())
					return { where->second, &GetMetaByTokenUnsafe(where->second) };
			}

			{
				std::unique_lock lock{ mDynamicLock };

				if (auto where = mIdentityMap.find(identity);
					where != mIdentityMap.end())
					return { where->second, &GetMetaByTokenUnsafe(where->second) };
				else
				{
					MDToken token = (MDToken)mDynamicMetaTokenTable.size();
					mDynamicMetaTokenTable.push_back(newMeta);
					mIdentityMap[identity] = token;

					return { token, newMeta };
				}
			}
		}
	};
}