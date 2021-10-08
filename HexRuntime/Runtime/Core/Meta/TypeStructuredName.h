#pragma once
#include "..\..\RuntimeAlias.h"
#include <string>
#include <string_view>
#include <vector>

namespace RTM
{
	/// <summary>
	/// Type need structured name for generic resolving
	/// </summary>
	class TypeStructuredName
	{
		std::wstring mOriginString;
		std::wstring_view mReferenceAssembly;
		std::wstring_view mNamespace;
		std::wstring_view mTypeName;
		std::vector<std::wstring_view> mTypeArguments;
		std::wstring_view mShortTypeName;
	public:
		static TypeStructuredName From(std::wstring_view originName);
	public:
		TypeStructuredName() = default;
		std::wstring_view GetReferenceAssembly()const;
		std::wstring_view GetNamespace()const;
		std::wstring_view GetTypeName()const;
		std::wstring_view GetShortTypeName()const;
		std::vector<std::wstring_view> const& GetTypeArguments()const;
		std::wstring_view GetFullyQualifiedName()const;
		std::wstring_view GetFullyQualifiedNameWithoutAssembly()const;
		TypeStructuredName InstantiateWith(std::vector<std::wstring_view> const& arguments);
	};
}