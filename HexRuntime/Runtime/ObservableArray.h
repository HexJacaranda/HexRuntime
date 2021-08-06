#pragma once
#include "RuntimeAlias.h"

namespace RT
{
	/// <summary>
	/// Only for immutable data referencing and iterating (mostly for meta data)
	/// </summary>
	/// <typeparam name="T"></typeparam>
	template<class T>
	struct ObservableArray
	{
		T* Values;
		Int32 Count;
	public:
		ForcedInline T* begin()const { return Values; }
		ForcedInline T* end()const { return Values + Count; }
		ForcedInline T& operator[](Int32 index) { return Values[index]; }
		ForcedInline T const& operator[](Int32 index)const { return Values[index]; }
		ObservableArray<T> Slice(Int32 count)const { return { Values, count }; }
		ObservableArray<T> Slice(Int32 index, Int32 count)const { return { Values + index, count }; }
	};
}