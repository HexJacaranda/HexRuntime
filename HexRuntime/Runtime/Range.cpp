#include "Range.h"

bool RT::IndexRange::IsEmpty() const
{
	return Count == 0;
}

std::wstring_view RT::IndexRange::operator[](std::wstring const& view) const
{
	return std::wstring_view{ view }.substr(Base, Count);
}