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
		std::unordered_map<std::wstring_view, Int32> mFQNMap;
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
			mFQNMap = std::move(another.mFQNMap);
			return *this;
		}
		DynamicTokenTable& operator=(DynamicTokenTable const&) = delete;

		DynamicTokenTable() {}
		DynamicTokenTable(ValueT* fixed, Int32 count) :mFixedCount(count), mFixedPart(fixed) {

		}

		bool DefineGenericInstantiation(KeyT const& key, ValueT* toAdd, ValueT*& returnValue, MDToken& defToken)
		{
			{
				std::shared_lock lock{ mDynLock };
				auto iterator = mDynMap.find(key);
				if (iterator != mDynMap.end())
				{
					Int32 index = iterator->second;
					returnValue = mDynPart[index];
					defToken = (MDToken)index;
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
					defToken = (MDToken)index;
					return false;
				}
				else
				{
					mDynPart.push_back(toAdd);
					Int32 index = mFixedCount + mDynCount++;
					mDynMap.insert(std::make_pair(key, index));
					returnValue = toAdd;
					defToken = (MDToken)index;
					return true;
				}
			}
		}
		bool DefineMetaMap(std::wstring_view fullyQualifiedName, MDToken token)
		{
			{
				std::shared_lock lock{ mDynLock };
				auto place = mFQNMap.find(fullyQualifiedName);
				if (place != mFQNMap.end())
					return false;
			}
			{
				std::unique_lock lock{ mDynLock };
				auto place = mFQNMap.find(fullyQualifiedName);
				if (place != mFQNMap.end())
					return false;
				else
				{
					mFQNMap.insert(std::make_pair(fullyQualifiedName, token));
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
			std::shared_lock lock{ mDynLock };
			return *mDynPart[token - mFixedCount];
		}

		MDToken const GetTokenByFQN(std::wstring_view fullyQualifiedName)const
		{
			return mFQNMap[fullyQualifiedName];
		}
		MDToken GetTokenByFQN(std::wstring_view fullyQualifiedName)
		{
			return mFQNMap[fullyQualifiedName];
		}

		ValueT& operator[](MDToken token) {
			return GetByToken(token);
		}
		ValueT const& operator[](MDToken token) const {
			return GetByToken(token);
		}
	};
}