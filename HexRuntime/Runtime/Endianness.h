#pragma once
#include "RuntimeAlias.h"
#include <utility>

namespace RT
{
	class Endianness
	{
	public:
		template<class T>
		static T Convert(T value)
		{
			if constexpr (std::is_same_v<T, UInt16>)
				return _byteswap_ushort(value);
			else if constexpr (std::is_same_v<T, UInt32>)
				return _byteswap_ulong(value);
			else if constexpr (std::is_same_v<T, UInt64>)
				return _byteswap_uint64(value);
			else
			{
				
			}
		}
	};
}