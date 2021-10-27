#pragma once
#include "RuntimeAlias.h"
#include <string>
#include <string_view>

namespace RT
{
	struct IndexRange
	{
		Int32 Base = 0;
		Int32 Count = 0;
	public:
		bool IsEmpty()const;
	public:
		std::wstring_view operator[](std::wstring const& view)const;
	};
}