#pragma once
#include "RuntimeAlias.h"
#include <vector>
#include <algorithm>
#include <iterator>

#define RANGE(VALUE) VALUE.begin(), VALUE.end()

namespace RT
{
	/// <summary>
	/// This is introduced for small scale set, which over-performs ordinary ordered_set or
	/// unordered_set
	/// </summary>
	/// <typeparam name="U"></typeparam>
	template<class U>
	class SmallSet
	{
		mutable std::vector<U> mSet;
	public:
		bool Contains(U const& value)const
		{
			return std::binary_search(RANGE(mSet), value);
		}
		void Remove(U const& value)
		{
			auto location = std::lower_bound(RANGE(mSet), value);
			if (location != mSet.end() && *location == value)
				mSet.erase(location);
		}
		void Add(U const& value)
		{
			auto location = std::lower_bound(RANGE(mSet), value);
			if (location != mSet.end() && *location == value)
				return;
			mSet.insert(location, value);		
		}
		Int32 Count()const
		{
			return mSet.size();
		}
		U& Front() {
			return mSet.front();
		}
		U const& Front()const {
			return mSet.front();
		}
		U& Back() {
			return mSet.back();
		}
		U const& Back()const {
			return mSet.back();
		}
	public:
		using iterator_type = typename std::vector<U>::iterator;
		using const_iterator_type = typename std::vector<U>::const_iterator;

		iterator_type begin() {
			return mSet.begin();
		}
		iterator_type end() {
			return mSet.end();
		}
		const_iterator_type begin() const {
			return mSet.cbegin();
		}
		const_iterator_type end() const {
			return mSet.cend();
		}
		const_iterator_type cbegin() const {
			return mSet.cbegin();
		}
		const_iterator_type cend() const {
			return mSet.cend();
		}
		iterator_type erase(const_iterator_type origin) {
			return mSet.erase(origin);
		}
	public:
		SmallSet() = default;
		SmallSet(SmallSet const&) = default;
		SmallSet(SmallSet &&) = default;
		SmallSet& operator=(SmallSet const&) = default;
		SmallSet& operator=(SmallSet&&) = default;
		~SmallSet() = default;
	public:
		SmallSet operator|(SmallSet const& another)
		{
			SmallSet value{};
			std::set_union(RANGE(mSet), RANGE(another.mSet), std::inserter(value.mSet, value.mSet.begin()));
			return value;
		}
		SmallSet operator&(SmallSet const& another)
		{
			SmallSet value{};
			std::set_intersection(RANGE(mSet), RANGE(another.mSet), std::inserter(value.mSet, value.mSet.begin()));
			return value;
		}
		SmallSet operator-(SmallSet const& another)
		{
			SmallSet value{};
			std::set_difference(RANGE(mSet), RANGE(another.mSet), std::inserter(value.mSet, value.mSet.begin()));
			return value;
		}
		bool operator== (SmallSet const& another)const
		{
			if (mSet.size() != another.mSet.size())
				return false;

			Int32 count = mSet.size();
			for (Int32 i = 0; i < count; ++i)
				if (mSet[i] != another.mSet[i])
					return false;
			return true;
		}
	};
}

#undef RANGE

