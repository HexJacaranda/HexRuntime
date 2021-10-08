#include "TypeStructuredName.h"

RTM::TypeStructuredName RTM::TypeStructuredName::From(std::wstring_view originName)
{
	//First assembly name or directly namespace
	
}

std::wstring_view RTM::TypeStructuredName::GetReferenceAssembly() const
{
	return mReferenceAssembly;
}

std::wstring_view RTM::TypeStructuredName::GetNamespace() const
{
	return mNamespace;
}

std::wstring_view RTM::TypeStructuredName::GetTypeName() const
{
	return mTypeName;
}

std::wstring_view RTM::TypeStructuredName::GetShortTypeName() const
{
	return mShortTypeName;
}

std::vector<std::wstring_view> const& RTM::TypeStructuredName::GetTypeArguments() const
{
	return mTypeArguments;
}

std::wstring_view RTM::TypeStructuredName::GetFullyQualifiedName() const
{
	return mOriginString;
}

std::wstring_view RTM::TypeStructuredName::GetFullyQualifiedNameWithoutAssembly() const
{
	std::wstring_view view{ mOriginString };
	if (mReferenceAssembly.empty())
		return view;

	//[Assembly]
	Int32 trimStartOffset = mReferenceAssembly.size() + 1;
	Int32 targetSize = mOriginString.size() - trimStartOffset;

	return view.substr(trimStartOffset, targetSize);
}

RTM::TypeStructuredName RTM::TypeStructuredName::InstantiateWith(std::vector<std::wstring_view> const& arguments)
{
	return TypeStructuredName();
}
