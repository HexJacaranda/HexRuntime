#pragma once
#include "RuntimeAlias.h"
#include <vector>
#include <unordered_map>
#include <shared_mutex>

namespace RT
{
	template<class KeyT, class ValueT, class HasherT, class EqualityT>
	class DynamicTokenTable
	{
		ValueT* mFixedPart = nullptr;
		Int32 mFixedCount = 0;
		Int32 mDynCount = 0;
		std::vector<ValueT*> mDynPart;
		std::unordered_map<KeyT, Int32, HasherT, EqualityT> mDynMap;
		std::shared_mutex mDynLock;
	public:
		DynamicTokenTable(DynamicTokenTable const&) = delete;
		DynamicTokenTable(DynamicTokenTable&&) = default;
		//Only allowed in constructing AssemblyContext
		DynamicTokenTable& operator=(DynamicTokenTable&& another)
		{
			mFixedCount = another.mFixedCount;
			mFixedPart = another.mFixedPart;
			mDynCount = another.mDynCount;
			mDynPart = std::move(another.mDynPart);
			mDynMap = std::move(another.mDynMap);
			return *this;
		}
		DynamicTokenTable& operator=(DynamicTokenTable const&) = delete;

		DynamicTokenTable() {}
		DynamicTokenTable(ValueT* fixed, Int32 count)
			:mFixedCount(count), mFixedPart(fixed) {
		}

		bool GetOrAdd(KeyT const& key, ValueT* toAdd, ValueT*& returnValue)
		{
			{
				std::shared_lock lock{ mDynLock };
				auto iterator = mDynMap.find(key);
				if (iterator != mDynMap.end())
				{
					Int32 index = iterator->second;
					returnValue = mDynPart[index];
					return false;
				}
			}

			{
				std::unique_lock lock{ mDynLock };
				auto iterator = mDynMap.find(key);
				if (iterator != mDynMap.end())
				{
					Int32 index = iterator->second;
					returnValue = mDynPart[index];
					return false;
				}
				else
				{
					mDynPart.push_back(toAdd);
					Int32 index = mDynCount++;
					mDynMap.insert(std::make_pair(key, index));
					returnValue = toAdd;
					return true;
				}
			}
		}

		ValueT const& GetByToken(MDToken token) const
		{
			return const_cast<DynamicTokenTable*>(this)->GetByToken(token);
		}

		ValueT& GetByToken(MDToken token)
		{
			//Fast path
			if (token < mFixedCount)
				return mFixedPart[token];
			std::shared_lock{ mDynLock };
			return *mDynPart[token - mFixedCount];
		}

		ValueT& operator[](MDToken token) {
			return GetByToken(token);
		}

		ValueT const& operator[](MDToken token) const {
			return GetByToken(token);
		}
	};
}